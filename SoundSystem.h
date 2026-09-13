#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class SoundSystem
{
public:
    struct LoadOptions
    {
        bool looping = false;
        int volumePercent = 100;
    };

    explicit SoundSystem(std::string basePath = {});
    ~SoundSystem();

    bool Initialize();
    void Shutdown();
    void Update(float deltaSeconds);

    bool LoadSound(const std::string &name, const std::string &path, const LoadOptions &options);
    bool PlaySound(const std::string &name, int volumePercentOverride = -1, bool forceLoopOverride = false, bool loopOverride = false);
    void StopSound(const std::string &name);
    void StopAll();

    bool PlayMusic(const std::string &name, int volumePercentOverride = -1);
    void StopMusic();

    // Gain (0.0-1.0) of the currently playing music stream, via SDL_GetAudioStreamGain.
    float GetMusicVolume() const;
    // Sets the currently playing music stream's gain via SDL_SetAudioStreamGain.
    void SetMusicVolume(float gain);

    // Volume (0-100) applied to future PlayMusic() calls that don't pass their own
    // volumePercentOverride, in place of the resource's authored default; used to keep a
    // user's music volume preference in effect across track changes.
    void SetDefaultMusicVolumePercent(int volumePercent);
    int GetDefaultMusicVolumePercent() const;

    bool HasSound(const std::string &name) const;

private:
    struct SoundResource
    {
        std::string name;
        std::string path;
        SDL_AudioSpec sourceSpec{};
        std::vector<std::uint8_t> pcm;
        bool looping = false;
        int volumePercent = 100;
    };

    struct ActivePlayback
    {
        SDL_AudioStream *stream = nullptr;
        const SoundResource *resource = nullptr;
        std::string resourceName;
        bool looping = false;
    };

    static float VolumeToGain(int volumePercent);
    std::string ResolvePath(const std::string &path) const;
    void DestroyPlayback(ActivePlayback &playback);

    std::string m_basePath;
    SDL_AudioDeviceID m_device = 0;
    SDL_AudioSpec m_deviceSpec{};
    int m_deviceSampleFrames = 0;
    int m_defaultMusicVolumePercent = 100;

    std::unordered_map<std::string, SoundResource> m_resources;
    std::vector<ActivePlayback> m_active;
    std::string m_currentMusic;
};
