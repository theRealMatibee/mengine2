#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <memory>
#include <string>
#include <unordered_map>

struct SDL_Window;

// Compiles and caches named pixel-shader pipelines for use with SpriteBuffer/Sprite, mirroring
// TextureManager's single-instance, ref-counted load/cache/unload model. Every pipeline built here
// shares SpriteBuffer's vertex shader/layout (TexturedQuad.vert) and samples a single texture, so
// it can be swapped in for the SpriteBuffer default pipeline via Sprite::SetShaderOverride().
class ShaderLibrary
{
public:
    struct ShaderResource
    {
        SDL_GPUGraphicsPipeline *pipeline = nullptr;
        SDL_GPUDevice *device = nullptr;
        std::string name;
        std::string file;
    };

    ShaderLibrary(SDL_GPUDevice *device, SDL_Window *window, std::string basePath);
    ~ShaderLibrary();

    ShaderLibrary(const ShaderLibrary &) = delete;
    ShaderLibrary &operator=(const ShaderLibrary &) = delete;

    // Compiles (or reuses an already-compiled pipeline for the same file) and registers it under
    // `name` for later lookup via Find(). Returns nullptr and logs to stderr on compile failure.
    std::shared_ptr<ShaderResource> LoadShader(const std::string &name, const std::string &file);

    // Looks up an already-registered shader by name. Returns nullptr if `name` was never loaded
    // (or has since been fully unloaded) -- callers should fall back to the default pipeline.
    std::shared_ptr<ShaderResource> Find(const std::string &name) const;

    void ClearCache();

private:
    struct CachedShader
    {
        std::weak_ptr<ShaderResource> weak;
    };

    std::shared_ptr<ShaderResource> CreatePipeline(const std::string &name, const std::string &file);

    SDL_GPUDevice *m_device = nullptr;
    SDL_Window *m_window = nullptr;
    std::string m_basePath;

    std::unordered_map<std::string, CachedShader> m_cacheByFile;               // dedup compiled pipelines by fragment shader file
    std::unordered_map<std::string, std::weak_ptr<ShaderResource>> m_byName;   // name -> resource lookup
};
