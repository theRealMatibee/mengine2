#include "pch.hpp"

#include "EngineRuntime.h"

#include "AppLoop.h"
#include "GUIManager.h"
#include "LoadingScreen.h"
#include "ModuleSystem.h"
#include "Registry.h"
#include "CrashMonitor.h"
#include "SdlBootstrap.h"
#include "SoundSystem.h"

#include <filesystem>
#include <iostream>
#include <memory>

int RunEngineRuntime(SdlBootstrapContext &sdl,
                     const std::function<void(ModuleSystem &)> &registerModules)
{
    CrashMonitor::Instance().RecordCheckpoint("Runtime::SoundSystem init");

    SoundSystem soundSystem;
    const bool soundInitialized = soundSystem.Initialize();
    if (!soundInitialized)
    {
        CrashMonitor::Instance().RecordLastError("SoundSystem initialization failed");
        std::cout << "[sound] audio init failed, continuing without sound\n";
    }

    // Constructed (not yet Loaded) before the loading screen is set up below - GameModule::
    // Initialize only requires a non-null GUIManager pointer, not a fully loaded one, and
    // LoadingScreen itself never touches it.
    GUIManager guiManager;

    ModuleSystem moduleSystem;

    // Queries the window's actual current pixel size (accounting for fullscreen/DPI scaling/a
    // non-default SdlWindowConfig) rather than assuming a hardcoded 960x540 - otherwise the very
    // first frames (the loading screen, before RunMainLoop's own per-frame recompute takes over)
    // render using a layout that doesn't match the real window, appearing in the wrong place.
    int initialWindowWidth = 960;
    int initialWindowHeight = 540;
    SDL_GetWindowSizeInPixels(sdl.window, &initialWindowWidth, &initialWindowHeight);
    GameModule::RenderLayout renderLayout = ComputeRenderLayout(initialWindowWidth, initialWindowHeight);

    // Engine-owned loading screen: registered/initialized up front (not by registerModules)
    // since it's generic infra, not game-specific. Never becomes ModuleSystem's "current"
    // module - it's driven manually via the Context callbacks below.
    moduleSystem.RegisterModule(ModuleSystem::ModuleType::LoadingScreen, std::make_unique<LoadingScreen>());
    GameModule *loadingScreenModule =
        moduleSystem.GetRegisteredModule(ModuleSystem::ModuleType::LoadingScreen);
    LoadingScreen *loadingScreen = static_cast<LoadingScreen *>(loadingScreenModule);

    std::function<void()> beginLoadingScreen = [loadingScreen]()
    {
        if (loadingScreen != nullptr)
        {
            loadingScreen->SetProgress(0.0f);
        }
    };

    Uint64 lastLoadingScreenPumpTicks = SDL_GetTicksNS();
    std::function<void(float)> pumpLoadingScreen =
        [&sdl, loadingScreenModule, loadingScreen, &renderLayout, lastLoadingScreenPumpTicks](
            float progress01) mutable
    {
        if (loadingScreenModule == nullptr)
        {
            return;
        }

        const Uint64 nowTicks = SDL_GetTicksNS();
        const float deltaSeconds =
            std::clamp(static_cast<float>(nowTicks - lastLoadingScreenPumpTicks) / 1000000000.0f, 0.0f, 0.25f);
        lastLoadingScreenPumpTicks = nowTicks;

        if (loadingScreen != nullptr)
        {
            loadingScreen->SetProgress(progress01);
        }
        loadingScreenModule->Update(deltaSeconds);
        PresentGameModuleFrame(sdl, loadingScreenModule, renderLayout);
    };

    std::function<void()> endLoadingScreen = []() {};

    // Set true by requestSoftReset() below; checked after RunMainLoop returns to decide whether
    // to report a normal exit code or the soft-reset sentinel.
    bool softResetRequested = false;
    std::function<void()> requestSoftReset = [&softResetRequested]()
    {
        softResetRequested = true;

        // Same mechanism MainMenu's own quit flow uses - cleanly unwinds RunMainLoop without
        // needing any new event-loop logic.
        SDL_Event quitEvent{};
        quitEvent.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quitEvent);
    };

    GameModule::Context context{
        sdl.textureManager.get(),
        &guiManager,
        soundInitialized ? &soundSystem : nullptr,
        moduleSystem.GetInputManager(),
        sdl.window,
        &renderLayout,
        &moduleSystem,
        sdl.shaderLibrary.get(),
        beginLoadingScreen,
        pumpLoadingScreen,
        endLoadingScreen,
        requestSoftReset
    };
    moduleSystem.SetContext(context);

    if (loadingScreenModule != nullptr)
    {
        loadingScreenModule->Initialize(context);
    }

    // Present the loading screen's first frame immediately, before GUIManager::Load or any
    // module's own resource loading runs - otherwise the swapchain has nothing intentional in
    // it yet and the window briefly shows undefined/garbage content on some window managers,
    // especially noticeable in fullscreen right after the window is created.
    if (context.pumpLoadingScreen)
    {
        context.pumpLoadingScreen(0.0f);
    }

    CrashMonitor::Instance().RecordCheckpoint("Runtime::GUI init");
    const std::filesystem::path guiConfigPath =
        (std::filesystem::path(sdl.basePath) / "assets/gui/GUIFILES/GUI.xml").lexically_normal();
    const bool guiLoaded = guiManager.Load(sdl.textureManager.get(),
                                           guiConfigPath.string(),
                                           soundInitialized ? &soundSystem : nullptr,
                                           sdl.shaderLibrary.get());
    if (guiLoaded)
    {
        guiManager.HideAllForms();
        std::cout << "[gui] loaded forms=" << guiManager.GetFormCount()
                  << " placeholders=" << guiManager.GetPlaceholderControlCount()
                  << "\n";
    }
    else
    {
        CrashMonitor::Instance().RecordLastError("Failed to load GUI");
        std::cout << "[gui] failed to load '" << guiConfigPath.string() << "'\n";
        return -1;
    }

    // Apply the persisted preference at startup, before any module gets a chance to touch it.
    guiManager.SetGamepadNavigationPreferenceEnabled(Registry::Instance().GetBoolValue("gamepadNavigation", true));

    // Applied before any module starts playing music, so the very first track already
    // respects the user's saved volume instead of each resource's authored default.
    if (soundInitialized)
    {
        soundSystem.SetDefaultMusicVolumePercent(Registry::Instance().GetIntValue("MusicVolume", 100));
    }

    GameModule::SetBrightness(Registry::Instance().GetIntValue("Brightness", 0));

    if (registerModules)
    {
        registerModules(moduleSystem);
    }

    if (!moduleSystem.StartAuto())
    {
        std::cout << "[module] failed to start initial module\n";
    }

    const int loopExitCode = RunMainLoop(sdl, moduleSystem, guiManager, renderLayout, guiLoaded);

    moduleSystem.Stop();
    soundSystem.Shutdown();
    guiManager.Unload();
    return softResetRequested ? kSoftResetExitCode : loopExitCode;
}

