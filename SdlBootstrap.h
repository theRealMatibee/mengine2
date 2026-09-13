#pragma once

#include <memory>

#include "SpriteBuffer.h"
#include "TextureManager.h"
#include "ShaderLibrary.h"

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;

struct SdlBootstrapContext
{
    SDL_Window *window = nullptr;
    SDL_GPUDevice *device = nullptr;
    SDL_GPUGraphicsPipeline *pipeline = nullptr;
    std::unique_ptr<SpriteBuffer> spriteBuffer;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<ShaderLibrary> shaderLibrary;
    const char *basePath = nullptr;
    bool sdlInitialized = false;
    bool shaderCrossInitialized = false;
    bool useSolidColorDebugShader = false;
};

// Initial window creation parameters, owned/supplied by the caller (main()) rather than
// hardcoded inside SdlBootstrap - the window may of course be resized/fullscreened afterwards.
struct SdlWindowConfig
{
    const char *title = "mengine2026";
    int minimumWidth = 960;
    int minimumHeight = 540;
    int width = 960;
    int height = 540;
    bool maximized = false;
    bool resizable = true;
    // Passed to SetVirtualResolution() by InitializeSdlAndGpu().
    int virtualWidth = 2272;
    int virtualHeight = 1278;
};

bool InitializeSdlAndGpu(SdlBootstrapContext &context, bool fullscreen, const SdlWindowConfig &windowConfig);
void ShutdownSdlAndGpu(SdlBootstrapContext &context);

// Picks an exclusive fullscreen display mode close to the window's current size (falling back
// to borderless fullscreen desktop mode, then to staying windowed) and applies it. Safe to call
// at any time; does nothing destructive on failure - the window is left/returned to windowed.
bool TryEnableFullscreen(SDL_Window *window);
