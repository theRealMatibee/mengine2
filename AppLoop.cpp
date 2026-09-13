#include "pch.hpp"

#include "AppLoop.h"

#include "GUIManager.h"
#include "ModuleSystem.h"
#include "SdlBootstrap.h"
#include "SpriteBuffer.h"
#include "Registry.h"

#include <algorithm>
#include <array>
#include <cmath>

static int g_virtualRenderWidth = 2272;
static int g_virtualRenderHeight = 1278;
static constexpr float kMaxSimulationDeltaSeconds = 1.0f / 30.0f;
static constexpr float kLargeDeltaHitchThresholdSeconds = 0.25f;

// Fullscreen-only mouse cursor auto-hide (module-agnostic, applies regardless of which
// GameModule is active): hidden by default on entering fullscreen, shown on mouse motion, and
// re-hidden after kFullscreenCursorIdleHideNs of no motion. Windowed mode is left untouched by
// this automatic behavior, but g_fullscreenCursorHidden is also reused by RequestCursorAutoHide()
// so any module can explicitly hide the cursor (in either windowed or fullscreen mode) and get
// the same "reappears on mouse motion" behavior for free.
static constexpr Uint64 kFullscreenCursorIdleHideNs = 5'000'000'000ULL;
static bool g_wasFullscreen = false;
static bool g_fullscreenCursorHidden = false;
static Uint64 g_lastMouseActivityTicks = 0;
static float g_stableFramesPerSecond = 0.0f;
static Uint64 g_lastWindowTextChangeTicks = 0;

const float GetStableFps()
{
    return g_stableFramesPerSecond;
}

void SetVirtualResolution(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    g_virtualRenderWidth = width;
    g_virtualRenderHeight = height;
}

void RequestCursorAutoHide()
{
    SDL_HideCursor();
    g_fullscreenCursorHidden = true;
    g_lastMouseActivityTicks = SDL_GetTicksNS();

    // Discard any motion events queued up before this call (e.g. real mouse movement during a
    // multi-second blocking load, before RunMainLoop's event loop was even running to drain
    // them) - otherwise the next SDL_PollEvent() would see that stale motion and immediately
    // undo the hide we just requested.
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_EVENT_MOUSE_MOTION);
}


static bool MapMouseToVirtual(const GameModule::RenderLayout &layout,
                              float mouseX,
                              float mouseY,
                              int *virtualXOut,
                              int *virtualYOut)
{
    if (virtualXOut == nullptr || virtualYOut == nullptr)
    {
        return false;
    }

    if (layout.viewportWidth <= 0 || layout.viewportHeight <= 0 ||
        layout.virtualWidth <= 0 || layout.virtualHeight <= 0)
    {
        return false;
    }

    const float localX = mouseX - static_cast<float>(layout.viewportX);
    const float localY = mouseY - static_cast<float>(layout.viewportY);
    if (localX < 0.0f || localY < 0.0f ||
        localX >= static_cast<float>(layout.viewportWidth) ||
        localY >= static_cast<float>(layout.viewportHeight))
    {
        return false;
    }

    const float normalizedX = localX / static_cast<float>(layout.viewportWidth);
    const float normalizedY = localY / static_cast<float>(layout.viewportHeight);
    const int virtualX = static_cast<int>(std::floor(normalizedX * static_cast<float>(layout.virtualWidth)));
    const int virtualY = static_cast<int>(std::floor(normalizedY * static_cast<float>(layout.virtualHeight)));

    *virtualXOut = std::clamp(virtualX, 0, layout.virtualWidth - 1);
    *virtualYOut = std::clamp(virtualY, 0, layout.virtualHeight - 1);
    return true;
}

void UpdateWindowTitle(SDL_Window *window, std::string& title, std::optional<bool> showFps)
{
    if (window == NULL)
    {
        return;
    }

    if (showFps.has_value() && showFps.value())
    {
        Uint64 nowTicks = SDL_GetTicksNS();
        if (nowTicks - g_lastWindowTextChangeTicks < 1000000000ULL ) // Limit updates to once per second
        {
            return;
        }
        g_lastWindowTextChangeTicks = nowTicks;

        char titleBuffer[320] = {};
        SDL_snprintf(titleBuffer,
                     sizeof(titleBuffer),
                     "%s | FPS %.1f",
                     title.c_str(),
                     g_stableFramesPerSecond);
        SDL_SetWindowTitle(window, titleBuffer);
    }
    else
    {
        SDL_SetWindowTitle(window, title.c_str());
    }
}

GameModule::RenderLayout ComputeRenderLayout(int windowWidth, int windowHeight)
{
    GameModule::RenderLayout layout;
    layout.virtualWidth = g_virtualRenderWidth;
    layout.virtualHeight = g_virtualRenderHeight;

    const int safeWindowWidth = std::max(1, windowWidth);
    const int safeWindowHeight = std::max(1, windowHeight);
    const float targetAspect = static_cast<float>(g_virtualRenderWidth) / static_cast<float>(g_virtualRenderHeight);

    // Prefer using the full window width; this produces centered top/bottom bars
    // when the display is wider than the target 16:9 aspect.
    layout.viewportWidth = safeWindowWidth;
    layout.viewportHeight = std::max(1, static_cast<int>(std::round(layout.viewportWidth / targetAspect)));
    layout.viewportX = 0;
    layout.viewportY = (safeWindowHeight - layout.viewportHeight) / 2;

    // If width-fit is too tall to fit the window (narrow displays), fall back to
    // height-fit so the full image remains visible.
    if (layout.viewportHeight > safeWindowHeight)
    {
        layout.viewportHeight = safeWindowHeight;
        layout.viewportWidth = std::max(1, static_cast<int>(std::round(layout.viewportHeight * targetAspect)));
        layout.viewportX = (safeWindowWidth - layout.viewportWidth) / 2;
        layout.viewportY = 0;
    }

    return layout;
}

int RunMainLoop(SdlBootstrapContext &sdl,
                ModuleSystem &moduleSystem,
                GUIManager &guiManager,
                GameModule::RenderLayout &renderLayout,
                bool guiLoaded)
{
    Uint64 previousTicks = SDL_GetTicksNS();
    Uint64 fpsWindowStartTicks = previousTicks;
    Uint32 fpsFrameCount = 0;
    float displayedFps = 0.0f;
    std::array<float, 1000> frameDrawTimesMs = {};
    size_t frameDrawTimeIndex = 0;
    size_t frameDrawTimeCount = 0;
    float frameDrawTimeSumMs = 0.0f;

    bool running = true;
    while (running)
    {
        const Uint64 currentTicks = SDL_GetTicksNS();

        const bool isFullscreen = (SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_FULLSCREEN) != 0;
        if (isFullscreen != g_wasFullscreen)
        {
            if (isFullscreen)
            {
                SDL_HideCursor();
                g_fullscreenCursorHidden = true;
                g_lastMouseActivityTicks = currentTicks; // don't instantly re-show from stale motion
            }
            else
            {
                SDL_ShowCursor();
                g_fullscreenCursorHidden = false;
            }
            g_wasFullscreen = isFullscreen;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            // Both are treated as "the app is exiting" (a window-manager close of the only
            // window doesn't necessarily also raise SDL_EVENT_QUIT unless SDL's default
            // quit-on-last-window-close hint stays enabled). Not marked as a clean exit here -
            // GameModule::Shutdown() (called later, once this loop returns) still needs to run
            // and would immediately overwrite the crash-trace checkpoint anyway; the true
            // "clean exit" marker is set once ALL teardown has finished, in main.cpp.
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION)
            {
                g_lastMouseActivityTicks = currentTicks;
                if (g_fullscreenCursorHidden)
                {
                    SDL_ShowCursor();
                    g_fullscreenCursorHidden = false;
                }
            }
            else if (event.type == SDL_EVENT_WINDOW_MAXIMIZED)
            {
                Registry::Instance().SetBoolValue("engineConfig.isMaximized", true);
            }
            else if (event.type == SDL_EVENT_WINDOW_RESTORED)
            {
                Registry::Instance().SetBoolValue("engineConfig.isMaximized", false);
            }
            else if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                if (!isFullscreen)
                {
                    int windowWidth = event.window.data1;
                    int windowHeight = event.window.data2;

                    Registry::Instance().SetIntValue("engineConfig.windowWidth", windowWidth);
                    Registry::Instance().SetIntValue("engineConfig.windowHeight", windowHeight);
                }
            }
        }

        if (isFullscreen && !g_fullscreenCursorHidden &&
            (currentTicks - g_lastMouseActivityTicks) >= kFullscreenCursorIdleHideNs)
        {
            SDL_HideCursor();
            g_fullscreenCursorHidden = true;
        }

        const float deltaSeconds = static_cast<float>(currentTicks - previousTicks) / 1000000000.0f;
        previousTicks = currentTicks;
        fpsFrameCount += 1;

        const Uint64 fpsWindowElapsedTicks = currentTicks - fpsWindowStartTicks;
        if (fpsWindowElapsedTicks >= 250000000ULL)
        {
            const float fpsWindowSeconds = static_cast<float>(fpsWindowElapsedTicks) / 1000000000.0f;
            if (fpsWindowSeconds > 0.0f)
            {
                displayedFps = static_cast<float>(fpsFrameCount) / fpsWindowSeconds;
            }
            fpsWindowStartTicks = currentTicks;
            fpsFrameCount = 0;
        }

        float clampedDelta = std::clamp(deltaSeconds, 0.0f, kMaxSimulationDeltaSeconds);
        if (deltaSeconds > kLargeDeltaHitchThresholdSeconds)
        {
            // Treat large hitches as a pause so physics doesn't tunnel through collisions.
            clampedDelta = kMaxSimulationDeltaSeconds;
        }

        moduleSystem.Update(clampedDelta);
        if (moduleSystem.GetCurrentModule() == nullptr)
        {
            running = false;
            continue;
        }
        guiManager.Update(clampedDelta);
        if (guiLoaded)
        {
            guiManager.UpdateGamepadNavigation(*moduleSystem.GetInputManager(), clampedDelta);
        }

        int windowPixelWidth = 0;
        int windowPixelHeight = 0;
        if (SDL_GetWindowSizeInPixels(sdl.window, &windowPixelWidth, &windowPixelHeight) &&
            windowPixelWidth > 0 && windowPixelHeight > 0)
        {
            renderLayout = ComputeRenderLayout(windowPixelWidth, windowPixelHeight);
            if (guiLoaded)
            {
                guiManager.SetViewportSize(renderLayout.virtualWidth, renderLayout.virtualHeight);
            }
        }

        float mouseXF = 0.0f;
        float mouseYF = 0.0f;
        const SDL_MouseButtonFlags mouseButtons = SDL_GetMouseState(&mouseXF, &mouseYF);
        const bool leftMouseDown = (mouseButtons & SDL_BUTTON_LMASK) != 0;

        int mouseVirtualX = -100000;
        int mouseVirtualY = -100000;
        const bool mouseInsideViewport = MapMouseToVirtual(renderLayout,
                                                           mouseXF,
                                                           mouseYF,
                                                           &mouseVirtualX,
                                                           &mouseVirtualY);
        moduleSystem.ProcessMouse(mouseVirtualX,
                                  mouseVirtualY,
                                  leftMouseDown && mouseInsideViewport);

        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(sdl.device);
        if (cmd == NULL)
        {
            break;
        }

        // Fragment cbuffer is padded to 16 bytes to match HLSL constant buffer register size.
        struct { float brightness; float time; float glowRadius; float padding; } fakeGammaUniforms =
            { SpriteBuffer::GetFakeGammaBrightnessNormalized(),
              static_cast<float>(currentTicks) / 1000000000.0f,
              SpriteBuffer::GetGlowRadiusPixels(),
              0.0f };
        SDL_PushGPUFragmentUniformData(cmd, 0, &fakeGammaUniforms, sizeof(fakeGammaUniforms));

        if (windowPixelWidth > 0 && windowPixelHeight > 0)
        {
            sdl.spriteBuffer->Begin();
            moduleSystem.Render(sdl.spriteBuffer.get());

            if (guiLoaded)
            {
                guiManager.RenderToBuffer(sdl.spriteBuffer.get());
            }

            sdl.spriteBuffer->Upload(cmd);
        }

        SDL_GPUTexture *swapchainTexture = NULL;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, sdl.window, &swapchainTexture, NULL, NULL))
        {
            SDL_CancelGPUCommandBuffer(cmd);
            break;
        }

        if (swapchainTexture != NULL)
        {
            SDL_GPUColorTargetInfo colorTargetInfo{};
            colorTargetInfo.texture = swapchainTexture;
            colorTargetInfo.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 1.0f };
            colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
            colorTargetInfo.cycle = false;

            SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(cmd, &colorTargetInfo, 1, NULL);
            SDL_GPUViewport viewport{};
            viewport.x = static_cast<float>(renderLayout.viewportX);
            viewport.y = static_cast<float>(renderLayout.viewportY);
            viewport.w = static_cast<float>(renderLayout.viewportWidth);
            viewport.h = static_cast<float>(renderLayout.viewportHeight);
            viewport.min_depth = 0.0f;
            viewport.max_depth = 1.0f;
            SDL_SetGPUViewport(renderPass, &viewport);

            SDL_Rect scissor{};
            scissor.x = renderLayout.viewportX;
            scissor.y = renderLayout.viewportY;
            scissor.w = renderLayout.viewportWidth;
            scissor.h = renderLayout.viewportHeight;
            SDL_SetGPUScissor(renderPass, &scissor);

            const Uint64 frameRenderStartTicks = SDL_GetTicksNS();
            if (sdl.useSolidColorDebugShader)
            {
                sdl.spriteBuffer->DrawNoSampler(renderPass, sdl.pipeline);
            }
            else
            {
                sdl.spriteBuffer->Draw(renderPass, sdl.pipeline);
            }
            const Uint64 frameRenderEndTicks = SDL_GetTicksNS();
            const float frameDrawTimeMs = static_cast<float>(frameRenderEndTicks - frameRenderStartTicks) / 1000000.0f;

            if (frameDrawTimeCount < frameDrawTimesMs.size())
            {
                frameDrawTimesMs[frameDrawTimeIndex] = frameDrawTimeMs;
                frameDrawTimeSumMs += frameDrawTimeMs;
                frameDrawTimeCount += 1;
            }
            else
            {
                frameDrawTimeSumMs -= frameDrawTimesMs[frameDrawTimeIndex];
                frameDrawTimesMs[frameDrawTimeIndex] = frameDrawTimeMs;
                frameDrawTimeSumMs += frameDrawTimeMs;
            }

            frameDrawTimeIndex = (frameDrawTimeIndex + 1) % frameDrawTimesMs.size();

            SDL_EndGPURenderPass(renderPass);
        }

        SDL_SubmitGPUCommandBuffer(cmd);
        sdl.spriteBuffer->OnUploadSubmitComplete();
    }

    return 0;
}

void PresentGameModuleFrame(SdlBootstrapContext &sdl,
                            GameModule *module,
                            const GameModule::RenderLayout &renderLayout)
{
    if (module == nullptr)
    {
        return;
    }

    SDL_PumpEvents(); // keep answering the window manager while a blocking OnInit pumps frames

    int windowPixelWidth = 0;
    int windowPixelHeight = 0;
    if (!SDL_GetWindowSizeInPixels(sdl.window, &windowPixelWidth, &windowPixelHeight) ||
        windowPixelWidth <= 0 || windowPixelHeight <= 0)
    {
        return;
    }

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(sdl.device);
    if (cmd == NULL)
    {
        return;
    }

    struct { float brightness; float time; float glowRadius; float padding; } fakeGammaUniforms =
        { SpriteBuffer::GetFakeGammaBrightnessNormalized(),
          static_cast<float>(SDL_GetTicksNS()) / 1000000000.0f,
          SpriteBuffer::GetGlowRadiusPixels(),
          0.0f };
    SDL_PushGPUFragmentUniformData(cmd, 0, &fakeGammaUniforms, sizeof(fakeGammaUniforms));

    sdl.spriteBuffer->Begin();
    module->Render(sdl.spriteBuffer.get());
    sdl.spriteBuffer->Upload(cmd);

    SDL_GPUTexture *swapchainTexture = NULL;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, sdl.window, &swapchainTexture, NULL, NULL))
    {
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }

    if (swapchainTexture != NULL)
    {
        SDL_GPUColorTargetInfo colorTargetInfo{};
        colorTargetInfo.texture = swapchainTexture;
        colorTargetInfo.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
        colorTargetInfo.cycle = false;

        SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(cmd, &colorTargetInfo, 1, NULL);

        SDL_GPUViewport viewport{};
        viewport.x = static_cast<float>(renderLayout.viewportX);
        viewport.y = static_cast<float>(renderLayout.viewportY);
        viewport.w = static_cast<float>(renderLayout.viewportWidth);
        viewport.h = static_cast<float>(renderLayout.viewportHeight);
        viewport.min_depth = 0.0f;
        viewport.max_depth = 1.0f;
        SDL_SetGPUViewport(renderPass, &viewport);

        SDL_Rect scissor{};
        scissor.x = renderLayout.viewportX;
        scissor.y = renderLayout.viewportY;
        scissor.w = renderLayout.viewportWidth;
        scissor.h = renderLayout.viewportHeight;
        SDL_SetGPUScissor(renderPass, &scissor);

        if (sdl.useSolidColorDebugShader)
        {
            sdl.spriteBuffer->DrawNoSampler(renderPass, sdl.pipeline);
        }
        else
        {
            sdl.spriteBuffer->Draw(renderPass, sdl.pipeline);
        }

        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(cmd);
    sdl.spriteBuffer->OnUploadSubmitComplete();
}
