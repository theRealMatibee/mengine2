#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <memory>
#include <string>
#include <unordered_map>

class TextureManager
{
public:
    struct TextureResource
    {
        SDL_GPUTexture *texture = nullptr;
        SDL_GPUSampler *sampler = nullptr;
        SDL_GPUDevice *device = nullptr;
        int width = 0;
        int height = 0;
        std::string key;
    };

    explicit TextureManager(SDL_GPUDevice *device, const std::string &basePath);
    ~TextureManager();

    TextureManager(const TextureManager &) = delete;
    TextureManager &operator=(const TextureManager &) = delete;

    std::shared_ptr<TextureResource> LoadTexture2D(const std::string &relativePath);
    std::shared_ptr<TextureResource> CreateSolidColorTexture(const SDL_FColor &color);

    SDL_GPUSampler *GetDefaultSampler() const;
    SDL_GPUDevice *GetDevice() const;

    void ResourceFree(std::shared_ptr<TextureResource> &resource);
    void ClearCache();

private:
    struct CachedTexture
    {
        std::weak_ptr<TextureResource> weak;
    };

    std::shared_ptr<TextureResource> CreateTexture2D(const std::string &relativePath);
    std::string ResolvePath(const std::string &relativePath) const;

    SDL_GPUDevice *m_device = nullptr;
    std::string m_basePath;
    SDL_GPUSampler *m_defaultSampler = nullptr;
    std::unordered_map<std::string, CachedTexture> m_cache;
};