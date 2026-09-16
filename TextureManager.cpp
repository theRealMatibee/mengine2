#include "TextureManager.h"

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <vector>

#include "CrashMonitor.h"

namespace {
std::string lowerExtension(const std::string &path)
{
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
    {
        return std::string();
    }

    std::string extension = path.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension;
}

SDL_Surface *loadImageFile(const std::string &fullPath)
{
    const std::string extension = lowerExtension(fullPath);
    if (extension == "bmp")
    {
        return SDL_LoadBMP(fullPath.c_str());
    }

    if (extension == "png")
    {
        return IMG_Load(fullPath.c_str());
    }

    SDL_Surface *surface = IMG_Load(fullPath.c_str());
    if (surface != nullptr)
    {
        return surface;
    }

    return SDL_LoadBMP(fullPath.c_str());
}
}

TextureManager::TextureManager(SDL_GPUDevice *device, const std::string &basePath)
    : m_device(device), m_basePath(basePath)
{
    SDL_GPUSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerCreateInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    samplerCreateInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    m_defaultSampler = SDL_CreateGPUSampler(m_device, &samplerCreateInfo);
}

TextureManager::~TextureManager()
{
    ClearCache();

    if (m_defaultSampler != nullptr)
    {
        SDL_ReleaseGPUSampler(m_device, m_defaultSampler);
        m_defaultSampler = nullptr;
    }
}

SDL_GPUSampler *TextureManager::GetDefaultSampler() const
{
    return m_defaultSampler;
}

SDL_GPUDevice *TextureManager::GetDevice() const
{
    return m_device;
}

std::string TextureManager::ResolvePath(const std::string &relativePath) const
{
    if (!relativePath.empty() && relativePath[0] == '/')
    {
        return relativePath;
    }

    if (relativePath.rfind("assets/", 0) == 0)
    {
        return m_basePath + relativePath;
    }

    return m_basePath + "assets/Content/Images/" + relativePath;
}

std::shared_ptr<TextureManager::TextureResource> TextureManager::CreateTexture2D(const std::string &relativePath)
{
    const std::string fullPath = ResolvePath(relativePath);

    SDL_Surface *surface = loadImageFile(fullPath);
    if (surface == nullptr)
    {
        std::string errorMessage = "Failed to load texture: " + fullPath;
        std::cerr << "[texture] failed to load '" << fullPath << "' (requested as '" << relativePath
                   << "'): " << SDL_GetError() << "\n";
        CrashMonitor::Instance().RecordLastError(errorMessage);
        return nullptr;
    }

    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    if (converted == nullptr)
    {
        std::string errorMessage = "Failed to convert texture: " + fullPath;
        std::cerr << "[texture] failed to convert '" << fullPath << "' (requested as '" << relativePath
                  << "'): " << SDL_GetError() << "\n";
        CrashMonitor::Instance().RecordLastError(errorMessage);
        return nullptr;
    }

    const int width = converted->w;
    const int height = converted->h;

    SDL_GPUTextureCreateInfo textureCreateInfo{};
    textureCreateInfo.type = SDL_GPU_TEXTURETYPE_2D;
    textureCreateInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    textureCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    textureCreateInfo.width = static_cast<Uint32>(width);
    textureCreateInfo.height = static_cast<Uint32>(height);
    textureCreateInfo.layer_count_or_depth = 1;
    textureCreateInfo.num_levels = 1;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(m_device, &textureCreateInfo);
    if (texture == nullptr)
    {
        std::string errorMessage = "Failed to create GPU texture: " + fullPath;
        std::cerr << "[texture] failed to create GPU texture '" << fullPath << "' (requested as '" << relativePath
                  << "'): " << SDL_GetError() << "\n";
        CrashMonitor::Instance().RecordLastError(errorMessage);
        SDL_DestroySurface(converted);
        return nullptr;
    }

    const size_t dataSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    SDL_GPUTransferBufferCreateInfo transferCreateInfo{};
    transferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferCreateInfo.size = static_cast<Uint32>(dataSize);

    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferCreateInfo);
    if (transferBuffer == nullptr)
    {
        std::string errorMessage = "Failed to create GPU transfer buffer for texture: " + fullPath;
        std::cerr << "[texture] failed to create GPU transfer buffer for '" << fullPath << "' (requested as '" << relativePath
                  << "'): " << SDL_GetError() << "\n";
        CrashMonitor::Instance().RecordLastError(errorMessage);
        SDL_ReleaseGPUTexture(m_device, texture);
        SDL_DestroySurface(converted);
        return nullptr;
    }

    void *mapped = SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);
    if (mapped == nullptr)
    {
        std::string errorMessage = "Failed to map GPU transfer buffer for texture: " + fullPath;
        std::cerr << "[texture] failed to map GPU transfer buffer for '" << fullPath << "' (requested as '" << relativePath
                  << "'): " << SDL_GetError() << "\n";
        CrashMonitor::Instance().RecordLastError(errorMessage);
        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
        SDL_ReleaseGPUTexture(m_device, texture);
        SDL_DestroySurface(converted);
        return nullptr;
    }

    SDL_memcpy(mapped, converted->pixels, dataSize);
    SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    if (cmd == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
        SDL_ReleaseGPUTexture(m_device, texture);
        SDL_DestroySurface(converted);
        return nullptr;
    }

    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo transferInfo{};
    transferInfo.transfer_buffer = transferBuffer;
    transferInfo.offset = 0;

    SDL_GPUTextureRegion textureRegion{};
    textureRegion.texture = texture;
    textureRegion.w = static_cast<Uint32>(width);
    textureRegion.h = static_cast<Uint32>(height);
    textureRegion.d = 1;

    SDL_UploadToGPUTexture(copyPass, &transferInfo, &textureRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
    SDL_DestroySurface(converted);

    auto resource = std::shared_ptr<TextureResource>(new TextureResource{ texture, m_defaultSampler, m_device, width, height, relativePath },
        [device = m_device](TextureResource *ptr) {
            if (ptr != nullptr)
            {
                if (ptr->texture != nullptr)
                {
                    SDL_ReleaseGPUTexture(device, ptr->texture);
                }
                delete ptr;
            }
        });

    return resource;
}

std::shared_ptr<TextureManager::TextureResource> TextureManager::LoadTexture2D(const std::string &relativePath)
{
    auto it = m_cache.find(relativePath);
    if (it != m_cache.end())
    {
        if (auto existing = it->second.weak.lock())
        {
            return existing;
        }
    }

    auto created = CreateTexture2D(relativePath);
    if (created != nullptr)
    {
        m_cache[relativePath].weak = created;
    }

    return created;
}

std::shared_ptr<TextureManager::TextureResource> TextureManager::CreateSolidColorTexture(const SDL_FColor &color)
{
    if (m_device == nullptr)
    {
        return nullptr;
    }

    SDL_GPUTextureCreateInfo textureCreateInfo{};
    textureCreateInfo.type = SDL_GPU_TEXTURETYPE_2D;
    textureCreateInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    textureCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    textureCreateInfo.width = 1;
    textureCreateInfo.height = 1;
    textureCreateInfo.layer_count_or_depth = 1;
    textureCreateInfo.num_levels = 1;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(m_device, &textureCreateInfo);
    if (texture == nullptr)
    {
        return nullptr;
    }

    const Uint8 r = static_cast<Uint8>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f + 0.5f);
    const Uint8 g = static_cast<Uint8>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f + 0.5f);
    const Uint8 b = static_cast<Uint8>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f + 0.5f);
    const Uint8 a = static_cast<Uint8>(std::clamp(color.a, 0.0f, 1.0f) * 255.0f + 0.5f);
    const Uint8 pixel[4] = { r, g, b, a };

    SDL_GPUTransferBufferCreateInfo transferCreateInfo{};
    transferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferCreateInfo.size = sizeof(pixel);

    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferCreateInfo);
    if (transferBuffer == nullptr)
    {
        SDL_ReleaseGPUTexture(m_device, texture);
        return nullptr;
    }

    void *mapped = SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);
    if (mapped == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
        SDL_ReleaseGPUTexture(m_device, texture);
        return nullptr;
    }

    SDL_memcpy(mapped, pixel, sizeof(pixel));
    SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    if (cmd == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
        SDL_ReleaseGPUTexture(m_device, texture);
        return nullptr;
    }

    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo transferInfo{};
    transferInfo.transfer_buffer = transferBuffer;
    transferInfo.offset = 0;

    SDL_GPUTextureRegion textureRegion{};
    textureRegion.texture = texture;
    textureRegion.w = 1;
    textureRegion.h = 1;
    textureRegion.d = 1;

    SDL_UploadToGPUTexture(copyPass, &transferInfo, &textureRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);

    auto resource = std::shared_ptr<TextureResource>(
        new TextureResource{ texture, m_defaultSampler, m_device, 1, 1, "debug-solid-color" },
        [device = m_device](TextureResource *ptr) {
            if (ptr != nullptr)
            {
                if (ptr->texture != nullptr)
                {
                    SDL_ReleaseGPUTexture(device, ptr->texture);
                }
                delete ptr;
            }
        });

    return resource;
}

void TextureManager::ClearCache()
{
    m_cache.clear();
}

void TextureManager::ResourceFree(std::shared_ptr<TextureResource> &resource)
{
    if (resource == nullptr)
    {
        return;
    }

    // Find and remove from cache
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
    {
        if (auto cached = it->second.weak.lock())
        {
            if (cached == resource)
            {
                m_cache.erase(it);
                break;
            }
        }
    }

    // Reset the shared_ptr, which decrements the reference count
    // If the reference count reaches zero, the custom deleter will be called
    // to release the GPU texture
    resource.reset();
}