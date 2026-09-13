#pragma once

#ifndef SDL3_NEW_ENGINE_GAMEMODULE_H
#define SDL3_NEW_ENGINE_GAMEMODULE_H

#include "GUIManager.h"
#include "InputManager.h"
#include "LegacyImage.h"
#include "ShaderLibrary.h"
#include "SoundSystem.h"

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

class SpriteBuffer;
class TextureManager;
class ModuleSystem;
struct SDL_Window;
namespace tinyxml2
{
class XMLElement;
}

class GameModule
{
public:
    // Active is the normal interactive state; TransitionIn/TransitionOut block GUI input
    // while a module's enter/exit effect plays out.
    enum class TransitionState
    {
        TransitionIn,
        Active,
        TransitionOut
    };

    struct RenderLayout
    {
        int viewportX = 0;
        int viewportY = 0;
        int viewportWidth = 960;
        int viewportHeight = 540;
        int virtualWidth = 2272;
        int virtualHeight = 1278;
    };

    struct Context
    {
        TextureManager *textureManager = nullptr;
        GUIManager *guiManager = nullptr;
        SoundSystem *soundSystem = nullptr;
        InputManager *inputManager = nullptr;
        SDL_Window *window = nullptr;
        const RenderLayout *renderLayout = nullptr;
        ModuleSystem *moduleSystem = nullptr;
        ShaderLibrary *shaderLibrary = nullptr;

        // Manual loading-screen hooks (set up once by EngineRuntime, independent of the normal
        // module Start/Update/Render flow). Intended usage from within another module's OnInit:
        // call beginLoadingScreen() first, call pumpLoadingScreen(progress) in place of a bare
        // SDL_PumpEvents() at each safe checkpoint of the blocking load, then endLoadingScreen()
        // right before returning. Empty/unset if the engine-level loading screen isn't available.
        std::function<void()> beginLoadingScreen;
        std::function<void(float)> pumpLoadingScreen;
        std::function<void()> endLoadingScreen;

        // Cleanly unwinds the current RunEngineRuntime() call (as if the app were quitting) and
        // signals main()'s caller to immediately start a brand new one - GUIManager, Translation,
        // SoundSystem and every registered module all end up freshly (re)constructed from
        // scratch, while SDL/the window/GPU device are left untouched. Intended for "everything
        // needs to reload from scratch" situations (e.g. the active language changed) rather than
        // a real quit; safe to call from anywhere, including GUI button callbacks, since it only
        // posts an event and sets a flag rather than touching any engine state directly.
        std::function<void()> requestSoftReset;
    };

    GameModule(std::string moduleName, std::string resourceListPath);
    virtual ~GameModule();

    bool Initialize(const Context &context);
    void Shutdown();
    void Resume();

    void Update(float deltaSeconds);
    void Render(SpriteBuffer *buffer) const;
    void ProcessMouse(int x, int y, bool leftButtonDown);

    bool IsFinished() const;
    const std::string &GetNextModuleName() const;
    const std::string &GetName() const;
    bool ShouldUnregisterAfterFinish() const;
    void ResetFlowState();

    LegacyImage *GetImage(const std::string &name) const;
    bool ShowForm(const std::string &name);
    bool RegisterButtonCallback(const std::string &formName,
                                const std::string &controlName,
                                std::function<void()> callback);
    bool PlaySound(const std::string &name, int volumePercentOverride = -1);
    bool PlaySong(const std::string &name, int volumePercentOverride = -1);

    // Global fake-gamma-correction brightness applied to every sprite on screen,
    // 0 (no adjustment) to 100 (maximum adjustment).
    static void SetBrightness(int level);
    static int GetBrightness();

    TransitionState GetTransitionState() const;
    float GetTransitionStateTime() const;
    bool IsGuiInteractionBlocked() const;

protected:
    virtual bool OnInit();
    virtual void OnEnter();
    virtual void OnUpdate(float deltaSeconds);
    virtual void OnRender(SpriteBuffer *buffer) const;
    virtual void OnShutdown();

    // Called when the engine reactivates an already-initialized module coming out of suspension
    // (e.g. an overlay module like OptionsMenu closing). OnEnter is not re-run in this case.
    virtual void OnResume();

    // Called when GUIManager's gamepad-navigation watchdog auto-disables itself (e.g. a noisy
    // or stuck analog stick); override to surface an error message to the player.
    virtual void OnGamepadNavigationFault();

    void EndModule(std::string nextModuleName = {});
    void SetUnregisterAfterFinish(bool shouldUnregister);

    // Starts the TransitionOut state (resetting its runtime) and remembers nextModuleName as the
    // target to EndModule() once the transition-out duration elapses. If the module has no
    // transition-out duration configured, EndModule(nextModuleName) runs immediately instead.
    void BeginTransitionOut(std::string nextModuleName);
    float GetTransitionInDuration() const;
    float GetTransitionOutDuration() const;

    const Context &GetContext() const;

private:
    bool LoadResourceList();
    bool LoadForms(tinyxml2::XMLElement *formsElement);
    bool LoadSprites(tinyxml2::XMLElement *spritesElement);
    bool LoadSounds(tinyxml2::XMLElement *soundsElement, bool asSongs);
    bool LoadShaders(tinyxml2::XMLElement *shadersElement);
    void ParseTransitions(const tinyxml2::XMLElement *transitionsElement);
    void AdvanceTransitionState(float deltaSeconds);

    // Resolves path relative to the executable's own directory and asks GUIManager to load it,
    // returning the form's internal registered <Name> value ("" on failure). Used for <Form
    // file="..."/> entries that give a full "assets/..." path to a module-owned form file.
    std::string LoadFormByExecutablePath(const std::string &path) const;

    std::string ResolveResourcePath(const std::string &path) const;
    static std::string NormalizePath(std::string path);

    std::string m_name;
    std::string m_resourceListPath;
    std::string m_resourceListDirectory;

    Context m_context{};
    bool m_initialized = false;
    bool m_finished = false;
    bool m_unregisterAfterFinish = false;
    std::string m_nextModuleName;

    std::vector<std::string> m_formNames;
    std::unordered_map<std::string, std::unique_ptr<LegacyImage>> m_images;
    std::unordered_map<std::string, std::string> m_soundByName;
    std::unordered_map<std::string, std::string> m_songByName;
    std::unordered_map<std::string, std::shared_ptr<ShaderLibrary::ShaderResource>> m_shadersByName;

    float m_transitionInDuration = 0.0f;
    float m_transitionOutDuration = 0.0f;
    TransitionState m_transitionState = TransitionState::Active;
    float m_transitionStateTime = 0.0f;
    std::string m_transitionOutTarget;
};

#endif

