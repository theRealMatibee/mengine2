#pragma once

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>

#include <string>
#include <unordered_map>
#include <vector>

// Opaque handle to a registered game action
using ActionHandle = int;
static constexpr ActionHandle InvalidActionHandle = -1;

class InputManager
{
public:
    InputManager();
    ~InputManager();

    // Register a named action and bind it to keyboard plus an optional joystick descriptor.
    // Examples: joy0_axis0_UP, joy0_axis1_DOWN, joy0_button0
    ActionHandle RegisterAction(const std::string &name,
                                SDL_Scancode key,
                                const std::string &defaultJoystickDescriptor = "");

    // Snapshot current keyboard state and compute per-action transitions
    void Update();

    bool IsDown(ActionHandle handle) const;
    bool IsJustPressed(ActionHandle handle) const;
    bool IsJustReleased(ActionHandle handle) const;

private:
    enum class JoystickBindingType
    {
        None,
        Button,
        Axis
    };

    struct JoystickBinding
    {
        bool isBound = false;
        std::string descriptor;
        int joystickIndex = -1;
        JoystickBindingType type = JoystickBindingType::None;
        int controlIndex = -1;
        int axisDirection = 0; // -1 for negative direction, +1 for positive direction
    };

    struct ActionState
    {
        std::string name;
        SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
        JoystickBinding joystickBinding;
        bool down = false;
        bool justPressed = false;
        bool justReleased = false;
    };

    struct PersistedBinding
    {
        SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
        std::string joystickDescriptor;
    };

    std::vector<ActionState> m_actions;
    std::unordered_map<std::string, ActionHandle> m_nameIndex;
    std::unordered_map<std::string, PersistedBinding> m_loadedBindings;
    std::string m_bindingFilePath;
    std::vector<SDL_JoystickID> m_joystickIds;
    std::unordered_map<SDL_JoystickID, SDL_Joystick *> m_openJoysticks;

    bool IsHandleValid(ActionHandle handle) const;
    bool IsJoystickBindingDown(const JoystickBinding &binding) const;
    static JoystickBinding ParseJoystickBinding(const std::string &descriptor);
    void InitializeBindingFilePath();
    void LoadBindingsFromXml();
    void EnumerateGameControllers() const;
    void RefreshOpenJoysticks();
    void CloseOpenJoysticks();
    bool SaveBindingsToXml() const;
    SDL_Scancode ResolveRegisteredKey(const std::string &name, SDL_Scancode fallbackKey) const;
    std::string ResolveRegisteredJoystickDescriptor(const std::string &name,
                                                    const std::string &fallbackDescriptor) const;
};
