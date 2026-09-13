#pragma once

#include "GameModule.h"
#include "InputManager.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

class SpriteBuffer;

class ModuleSystem
{
public:
    enum class ModuleType
    {
        Splash = 0,
        Intro = 1,
        MainMenu = 2,
        Game = 3,
        Pause = 4,
        Outro = 5,
        OptionsMenu = 6,
        SaveSlots = 7,
        // Not part of the normal Start/Update/Render flow - see GameModule::Context's
        // begin/pump/endLoadingScreen callbacks for how this is actually driven.
        LoadingScreen = 8,
        Count = 9
    };

    ModuleSystem() = default;
    ~ModuleSystem();

    void SetContext(const GameModule::Context &context);
    void RegisterModule(ModuleType type, std::unique_ptr<GameModule> module);
    void UnregisterModule(ModuleType type);
    GameModule *GetRegisteredModule(ModuleType type) const;

    bool Start(ModuleType moduleType);
    bool StartAuto();
    void Stop();

    void Update(float deltaSeconds);
    void ProcessMouse(int x, int y, bool leftButtonDown);
    void Render(SpriteBuffer *buffer) const;

    GameModule *GetCurrentModule() const;
    ModuleType GetCurrentModuleType() const;

    InputManager *GetInputManager() { return &m_inputManager; }

private:
    static bool IsValidType(ModuleType type);
    static int ToIndex(ModuleType type);
    static const char *ToDebugName(ModuleType type);

    bool Activate(ModuleType moduleType);
    ModuleType FindFirstRegistered() const;
    ModuleType FindNextRegisteredAfter(ModuleType moduleType) const;
    ModuleType FindTypeByModuleName(const std::string &moduleName) const;
    static bool IsOverlayType(ModuleType type);

    GameModule::Context m_context{};
    std::array<std::unique_ptr<GameModule>, static_cast<size_t>(ModuleType::Count)> m_modules{};
    ModuleType m_currentType = ModuleType::Count;
    std::vector<ModuleType> m_suspendedStack;
    InputManager m_inputManager;
};
