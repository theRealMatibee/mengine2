#include "SoundSystem.h"

#include "dr_mp3.h"

#include <algorithm>
#include <filesystem>

SoundSystem::SoundSystem(std::string basePath)
    : m_basePath(std::move(basePath))
{
}

SoundSystem::~SoundSystem()
{
    Shutdown();
}

bool SoundSystem::Initialize()
{
    if (m_device != 0)
    {
        return true;
    }

    m_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (m_device == 0)
    {
        SDL_Log("[sound] SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }

    if (!SDL_GetAudioDeviceFormat(m_device, &m_deviceSpec, &m_deviceSampleFrames))
    {
        SDL_Log("[sound] SDL_GetAudioDeviceFormat failed: %s", SDL_GetError());
        SDL_CloseAudioDevice(m_device);
        m_device = 0;
        return false;
    }

    if (!SDL_ResumeAudioDevice(m_device))
    {
        SDL_Log("[sound] SDL_ResumeAudioDevice failed: %s", SDL_GetError());
        SDL_CloseAudioDevice(m_device);
        m_device = 0;
        return false;
    }

    return true;
}

void SoundSystem::Shutdown()
{
    StopAll();
    m_resources.clear();
    m_currentMusic.clear();

    if (m_device != 0)
    {
        SDL_CloseAudioDevice(m_device);
        m_device = 0;
    }
}

void SoundSystem::Update(float deltaSeconds)
{
    (void) deltaSeconds;

    for (auto it = m_active.begin(); it != m_active.end();)
    {
        if (it->stream == nullptr || it->resource == nullptr)
        {
            it = m_active.erase(it);
            continue;
        }

        const int queued = SDL_GetAudioStreamQueued(it->stream);

        if (it->looping)
        {
            if (queued <= static_cast<int>(it->resource->pcm.size()))
            {
                SDL_PutAudioStreamData(it->stream,
                                       it->resource->pcm.data(),
                                       static_cast<int>(it->resource->pcm.size()));
            }
            ++it;
            continue;
        }

        if (queued <= 0)
        {
            DestroyPlayback(*it);
            it = m_active.erase(it);
            continue;
        }

        ++it;
    }
}

bool SoundSystem::LoadSound(const std::string &name, const std::string &path, const LoadOptions &options)
{
    if (name.empty())
    {
        return false;
    }

    const std::string fullPath = ResolvePath(path);
    const std::string extension = std::filesystem::path(fullPath).extension().string();
    const bool isMp3 = extension.size() == 4 &&
                       (extension[1] == 'm' || extension[1] == 'M') &&
                       (extension[2] == 'p' || extension[2] == 'P') &&
                       extension[3] == '3';

    SDL_AudioSpec wavSpec{};
    std::vector<std::uint8_t> pcmBytes;

    if (isMp3)
    {
        drmp3_config config{};
        drmp3_uint64 frameCount = 0;
        drmp3_int16 *pcmFrames = drmp3_open_file_and_read_pcm_frames_s16(fullPath.c_str(), &config, &frameCount, nullptr);
        if (pcmFrames == nullptr || config.channels <= 0)
        {
            return false;
        }

        wavSpec.format = SDL_AUDIO_S16;
        wavSpec.channels = static_cast<int>(config.channels);
        wavSpec.freq = static_cast<int>(config.sampleRate);

        const std::size_t byteCount = static_cast<std::size_t>(frameCount) * config.channels * sizeof(drmp3_int16);
        const auto *bytes = reinterpret_cast<const std::uint8_t *>(pcmFrames);
        pcmBytes.assign(bytes, bytes + byteCount);

        drmp3_free(pcmFrames, nullptr);
    }
    else
    {
        Uint8 *wavBuffer = nullptr;
        Uint32 wavLength = 0;
        if (!SDL_LoadWAV(fullPath.c_str(), &wavSpec, &wavBuffer, &wavLength))
        {
            return false;
        }

        pcmBytes.assign(wavBuffer, wavBuffer + wavLength);
        SDL_free(wavBuffer);
    }

    SoundResource resource{};
    resource.name = name;
    resource.path = fullPath;
    resource.sourceSpec = wavSpec;
    resource.pcm = std::move(pcmBytes);
    resource.looping = options.looping;
    resource.volumePercent = std::clamp(options.volumePercent, 0, 100);

    m_resources[name] = std::move(resource);
    return true;
}

bool SoundSystem::PlaySound(const std::string &name,
                            int volumePercentOverride,
                            bool forceLoopOverride,
                            bool loopOverride)
{
    if (m_device == 0)
    {
        return false;
    }

    const auto it = m_resources.find(name);
    if (it == m_resources.end() || it->second.pcm.empty())
    {
        return false;
    }

    const SoundResource &resource = it->second;

    SDL_AudioStream *stream = SDL_CreateAudioStream(&resource.sourceSpec, &m_deviceSpec);
    if (stream == nullptr)
    {
        return false;
    }

    if (!SDL_BindAudioStream(m_device, stream))
    {
        SDL_DestroyAudioStream(stream);
        return false;
    }

    const int volumePercent =
        (volumePercentOverride >= 0)
            ? std::clamp(volumePercentOverride, 0, 100)
            : resource.volumePercent;

    SDL_SetAudioStreamGain(stream, VolumeToGain(volumePercent));

    if (!SDL_PutAudioStreamData(stream,
                                resource.pcm.data(),
                                static_cast<int>(resource.pcm.size())))
    {
        SDL_DestroyAudioStream(stream);
        return false;
    }

    ActivePlayback playback{};
    playback.stream = stream;
    playback.resource = &resource;
    playback.resourceName = name;
    playback.looping = forceLoopOverride ? loopOverride : resource.looping;
    m_active.push_back(playback);

    return true;
}

void SoundSystem::StopSound(const std::string &name)
{
    for (auto it = m_active.begin(); it != m_active.end();)
    {
        if (it->resourceName == name)
        {
            DestroyPlayback(*it);
            it = m_active.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (m_currentMusic == name)
    {
        m_currentMusic.clear();
    }
}

void SoundSystem::StopAll()
{
    for (ActivePlayback &playback : m_active)
    {
        DestroyPlayback(playback);
    }
    m_active.clear();
    m_currentMusic.clear();
}

bool SoundSystem::PlayMusic(const std::string &name, int volumePercentOverride)
{
    StopMusic();
    const int resolvedVolume = (volumePercentOverride >= 0) ? volumePercentOverride : m_defaultMusicVolumePercent;
    if (!PlaySound(name, resolvedVolume, true, true))
    {
        return false;
    }

    m_currentMusic = name;
    return true;
}

void SoundSystem::StopMusic()
{
    if (!m_currentMusic.empty())
    {
        StopSound(m_currentMusic);
    }
}

float SoundSystem::GetMusicVolume() const
{
    for (const ActivePlayback &playback : m_active)
    {
        if (playback.resourceName == m_currentMusic && playback.stream != nullptr)
        {
            return SDL_GetAudioStreamGain(playback.stream);
        }
    }

    return 0.0f;
}

void SoundSystem::SetMusicVolume(float gain)
{
    for (ActivePlayback &playback : m_active)
    {
        if (playback.resourceName == m_currentMusic && playback.stream != nullptr)
        {
            SDL_SetAudioStreamGain(playback.stream, gain);
        }
    }
}

void SoundSystem::SetDefaultMusicVolumePercent(int volumePercent)
{
    m_defaultMusicVolumePercent = std::clamp(volumePercent, 0, 100);
}

int SoundSystem::GetDefaultMusicVolumePercent() const
{
    return m_defaultMusicVolumePercent;
}

bool SoundSystem::HasSound(const std::string &name) const
{
    return m_resources.find(name) != m_resources.end();
}

float SoundSystem::VolumeToGain(int volumePercent)
{
    const int clamped = std::clamp(volumePercent, 0, 100);
    return static_cast<float>(clamped) / 100.0f;
}

std::string SoundSystem::ResolvePath(const std::string &path) const
{
    if (path.empty())
    {
        return path;
    }

    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    const std::filesystem::path p(normalized);
    if (p.is_absolute())
    {
        return p.lexically_normal().string();
    }

    return (std::filesystem::path(m_basePath) / normalized).lexically_normal().string();
}

void SoundSystem::DestroyPlayback(ActivePlayback &playback)
{
    if (playback.stream != nullptr)
    {
        SDL_DestroyAudioStream(playback.stream);
        playback.stream = nullptr;
    }
}
