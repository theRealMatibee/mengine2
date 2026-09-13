#include "InputManager.h"

#include "Registry.h"
#include "tinyxml2.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_keyboard.h>

#include <algorithm>
#include <cctype>

namespace
{
constexpr const char *kBindingFileName = "inputbindings.xml";
constexpr Sint16 kJoystickAxisThreshold = 16000;

bool IsAsciiDigit(char c)
{
    return c >= '0' && c <= '9';
}

bool StartsWithAt(const std::string &text, size_t position, const char *token)
{
    const size_t tokenLength = SDL_strlen(token);
    return text.compare(position, tokenLength, token) == 0;
}

bool ParseUnsignedInteger(const std::string &text, size_t &cursor, int &valueOut)
{
    if (cursor >= text.size() || !IsAsciiDigit(text[cursor]))
    {
        return false;
    }

    int value = 0;
    while (cursor < text.size() && IsAsciiDigit(text[cursor]))
    {
        value = (value * 10) + static_cast<int>(text[cursor] - '0');
        ++cursor;
    }

    valueOut = value;
    return true;
}
}

InputManager::InputManager()
{
    InitializeBindingFilePath();
    LoadBindingsFromXml();
    EnumerateGameControllers();
}

InputManager::~InputManager()
{
    if (SaveBindingsToXml() && !m_bindingFilePath.empty())
    {
        SDL_Log("[input] wrote input bindings on shutdown: %s", m_bindingFilePath.c_str());
    }

    CloseOpenJoysticks();
}

ActionHandle InputManager::RegisterAction(const std::string &name,
                                          SDL_Scancode key,
                                          const std::string &defaultJoystickDescriptor)
{
    auto it = m_nameIndex.find(name);
    if (it != m_nameIndex.end())
    {
        ActionState &action = m_actions[it->second];

        // If a persisted binding has no joystick descriptor yet, adopt the
        // provided default descriptor so newly introduced defaults can persist.
        if (action.joystickBinding.descriptor.empty() && !defaultJoystickDescriptor.empty())
        {
            const JoystickBinding defaultBinding = ParseJoystickBinding(defaultJoystickDescriptor);
            action.joystickBinding = defaultBinding;

            PersistedBinding persistedBinding;
            persistedBinding.key = action.key;
            persistedBinding.joystickDescriptor = defaultBinding.descriptor;
            m_loadedBindings[name] = persistedBinding;
            SaveBindingsToXml();
        }

        return it->second;
    }

    const SDL_Scancode resolvedKey = ResolveRegisteredKey(name, key);
    const std::string resolvedJoystickDescriptor =
        ResolveRegisteredJoystickDescriptor(name, defaultJoystickDescriptor);
    const JoystickBinding joystickBinding = ParseJoystickBinding(resolvedJoystickDescriptor);

    const ActionHandle handle = static_cast<ActionHandle>(m_actions.size());

    ActionState action;
    action.name = name;
    action.key = resolvedKey;
    action.joystickBinding = joystickBinding;
    m_actions.push_back(action);

    m_nameIndex[name] = handle;

    PersistedBinding persistedBinding;
    persistedBinding.key = resolvedKey;
    persistedBinding.joystickDescriptor = joystickBinding.descriptor;
    m_loadedBindings[name] = persistedBinding;

    SaveBindingsToXml();

    return handle;
}

void InputManager::Update()
{
    int numKeys = 0;
    const bool *keyState = SDL_GetKeyboardState(&numKeys);
    RefreshOpenJoysticks();

    for (ActionState &action : m_actions)
    {
        const int sc = static_cast<int>(action.key);
        const bool keyboardDown = (sc >= 0 && sc < numKeys) && keyState[sc];
        const bool joystickDown = IsJoystickBindingDown(action.joystickBinding);
        const bool currentlyDown = keyboardDown || joystickDown;

        action.justPressed = currentlyDown && !action.down;
        action.justReleased = !currentlyDown && action.down;
        action.down = currentlyDown;
    }
}

bool InputManager::IsDown(ActionHandle handle) const
{
    if (!IsHandleValid(handle)) return false;
    return m_actions[handle].down;
}

bool InputManager::IsJustPressed(ActionHandle handle) const
{
    if (!IsHandleValid(handle)) return false;
    return m_actions[handle].justPressed;
}

bool InputManager::IsJustReleased(ActionHandle handle) const
{
    if (!IsHandleValid(handle)) return false;
    return m_actions[handle].justReleased;
}

bool InputManager::IsHandleValid(ActionHandle handle) const
{
    return handle >= 0 && handle < static_cast<ActionHandle>(m_actions.size());
}

bool InputManager::IsJoystickBindingDown(const JoystickBinding &binding) const
{
    if (!binding.isBound)
    {
        return false;
    }

    if (binding.joystickIndex < 0 || binding.joystickIndex >= static_cast<int>(m_joystickIds.size()))
    {
        return false;
    }

    const SDL_JoystickID joystickId = m_joystickIds[static_cast<size_t>(binding.joystickIndex)];
    SDL_Joystick *joystick = nullptr;

    const auto openIt = m_openJoysticks.find(joystickId);
    if (openIt != m_openJoysticks.end())
    {
        joystick = openIt->second;
    }

    if (joystick == nullptr)
    {
        return false;
    }

    if (binding.type == JoystickBindingType::Button)
    {
        return SDL_GetJoystickButton(joystick, binding.controlIndex);
    }

    if (binding.type == JoystickBindingType::Axis)
    {
        const Sint16 axisValue = SDL_GetJoystickAxis(joystick, binding.controlIndex);
        if (binding.axisDirection < 0)
        {
            return axisValue <= -kJoystickAxisThreshold;
        }
        if (binding.axisDirection > 0)
        {
            return axisValue >= kJoystickAxisThreshold;
        }
    }

    return false;
}

InputManager::JoystickBinding InputManager::ParseJoystickBinding(const std::string &descriptor)
{
    JoystickBinding binding;
    if (descriptor.empty())
    {
        return binding;
    }

    std::string lowerDescriptor = descriptor;
    std::transform(lowerDescriptor.begin(), lowerDescriptor.end(), lowerDescriptor.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    size_t cursor = 0;
    if (!StartsWithAt(lowerDescriptor, cursor, "joy"))
    {
        return binding;
    }
    cursor += 3;

    int joystickIndex = -1;
    if (!ParseUnsignedInteger(lowerDescriptor, cursor, joystickIndex))
    {
        return binding;
    }
    if (cursor >= lowerDescriptor.size() || lowerDescriptor[cursor] != '_')
    {
        return binding;
    }
    ++cursor;

    if (StartsWithAt(lowerDescriptor, cursor, "button"))
    {
        cursor += 6;
        int buttonIndex = -1;
        if (!ParseUnsignedInteger(lowerDescriptor, cursor, buttonIndex))
        {
            return binding;
        }
        if (cursor != lowerDescriptor.size())
        {
            return binding;
        }

        binding.isBound = true;
        binding.descriptor = lowerDescriptor;
        binding.joystickIndex = joystickIndex;
        binding.type = JoystickBindingType::Button;
        binding.controlIndex = buttonIndex;
        return binding;
    }

    if (StartsWithAt(lowerDescriptor, cursor, "axis"))
    {
        cursor += 4;
        int axisIndex = -1;
        if (!ParseUnsignedInteger(lowerDescriptor, cursor, axisIndex))
        {
            return binding;
        }
        if (cursor >= lowerDescriptor.size() || lowerDescriptor[cursor] != '_')
        {
            return binding;
        }
        ++cursor;

        int axisDirection = 0;
        size_t suffixLength = 0;
        if (StartsWithAt(lowerDescriptor, cursor, "up"))
        {
            axisDirection = -1;
            suffixLength = 2;
        }
        else if (StartsWithAt(lowerDescriptor, cursor, "left"))
        {
            axisDirection = -1;
            suffixLength = 4;
        }
        else if (StartsWithAt(lowerDescriptor, cursor, "neg"))
        {
            axisDirection = -1;
            suffixLength = 3;
        }
        else if (StartsWithAt(lowerDescriptor, cursor, "down"))
        {
            axisDirection = 1;
            suffixLength = 4;
        }
        else if (StartsWithAt(lowerDescriptor, cursor, "right"))
        {
            axisDirection = 1;
            suffixLength = 5;
        }
        else if (StartsWithAt(lowerDescriptor, cursor, "pos"))
        {
            axisDirection = 1;
            suffixLength = 3;
        }
        else
        {
            return binding;
        }

        cursor += suffixLength;
        if (cursor != lowerDescriptor.size())
        {
            return binding;
        }

        binding.isBound = true;
        binding.descriptor = lowerDescriptor;
        binding.joystickIndex = joystickIndex;
        binding.type = JoystickBindingType::Axis;
        binding.controlIndex = axisIndex;
        binding.axisDirection = axisDirection;
        return binding;
    }

    return binding;
}

void InputManager::EnumerateGameControllers() const
{
    int joystickCount = 0;
    SDL_JoystickID *joystickIds = SDL_GetJoysticks(&joystickCount);
    if (joystickIds == nullptr)
    {
        if (joystickCount == 0)
        {
            SDL_Log("[input] no joysticks detected during initialization");
        }
        else
        {
            SDL_Log("[input] failed to enumerate joysticks: %s", SDL_GetError());
        }
        return;
    }

    SDL_Log("[input] detected %d joystick%s during initialization", joystickCount, (joystickCount == 1) ? "" : "s");

    for (int index = 0; index < joystickCount; ++index)
    {
        const SDL_JoystickID joystickId = joystickIds[index];
        const bool isGamepad = SDL_IsGamepad(joystickId);

        const char *joystickName = SDL_GetJoystickNameForID(joystickId);
        if (joystickName == nullptr)
        {
            joystickName = "<unknown>";
        }

        if (isGamepad)
        {
            const char *gamepadName = SDL_GetGamepadNameForID(joystickId);
            if (gamepadName == nullptr)
            {
                gamepadName = joystickName;
            }

            SDL_Log("[input] joystick %d: %s (instance id=%d, gamepad profile=%s)",
                    index,
                    joystickName,
                    static_cast<int>(joystickId),
                    gamepadName);
        }
        else
        {
            SDL_Log("[input] joystick %d: %s (instance id=%d, no gamepad mapping)",
                    index,
                    joystickName,
                    static_cast<int>(joystickId));
        }
    }

    SDL_free(joystickIds);
}

void InputManager::RefreshOpenJoysticks()
{
    int joystickCount = 0;
    SDL_JoystickID *joystickIds = SDL_GetJoysticks(&joystickCount);

    if (joystickIds == nullptr)
    {
        m_joystickIds.clear();
        CloseOpenJoysticks();
        return;
    }

    std::vector<SDL_JoystickID> latestIds;
    latestIds.reserve(static_cast<size_t>(joystickCount));
    for (int index = 0; index < joystickCount; ++index)
    {
        latestIds.push_back(joystickIds[index]);
    }
    SDL_free(joystickIds);

    for (auto it = m_openJoysticks.begin(); it != m_openJoysticks.end();)
    {
        const bool stillConnected = std::find(latestIds.begin(), latestIds.end(), it->first) != latestIds.end();
        if (!stillConnected)
        {
            if (it->second != nullptr)
            {
                SDL_CloseJoystick(it->second);
            }
            it = m_openJoysticks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (const SDL_JoystickID joystickId : latestIds)
    {
        if (m_openJoysticks.find(joystickId) == m_openJoysticks.end())
        {
            m_openJoysticks[joystickId] = SDL_OpenJoystick(joystickId);
        }
    }

    m_joystickIds = std::move(latestIds);
}

void InputManager::CloseOpenJoysticks()
{
    for (const auto &entry : m_openJoysticks)
    {
        if (entry.second != nullptr)
        {
            SDL_CloseJoystick(entry.second);
        }
    }

    m_openJoysticks.clear();
    m_joystickIds.clear();
}

void InputManager::InitializeBindingFilePath()
{
    const std::string &storageFolderPath = Registry::GetStorageFolderPath();
    if (storageFolderPath.empty())
    {
        return;
    }

    m_bindingFilePath = storageFolderPath + kBindingFileName;
}

void InputManager::LoadBindingsFromXml()
{
    if (m_bindingFilePath.empty())
    {
        return;
    }

    tinyxml2::XMLDocument document;
    const tinyxml2::XMLError loadResult = document.LoadFile(m_bindingFilePath.c_str());
    if (loadResult != tinyxml2::XML_SUCCESS)
    {
        return;
    }

    const tinyxml2::XMLElement *rootElement = document.FirstChildElement("InputBindings");
    if (rootElement == nullptr)
    {
        return;
    }

    for (const tinyxml2::XMLElement *actionElement = rootElement->FirstChildElement("Action");
         actionElement != nullptr;
         actionElement = actionElement->NextSiblingElement("Action"))
    {
        const char *actionName = actionElement->Attribute("name");
        int scancodeValue = static_cast<int>(SDL_SCANCODE_UNKNOWN);
        if (actionName == nullptr || actionName[0] == '\0')
        {
            continue;
        }

        PersistedBinding binding;
        if (actionElement->QueryIntAttribute("scancode", &scancodeValue) == tinyxml2::XML_SUCCESS)
        {
            binding.key = static_cast<SDL_Scancode>(scancodeValue);
        }

        const char *joystickDescriptor = actionElement->Attribute("joystick");
        if (joystickDescriptor != nullptr)
        {
            binding.joystickDescriptor = joystickDescriptor;
        }

        m_loadedBindings[actionName] = binding;
    }
}

bool InputManager::SaveBindingsToXml() const
{
    if (m_bindingFilePath.empty())
    {
        return false;
    }

    tinyxml2::XMLDocument document;
    tinyxml2::XMLElement *rootElement = document.NewElement("InputBindings");
    document.InsertEndChild(rootElement);

    std::unordered_map<std::string, PersistedBinding> bindingsToWrite = m_loadedBindings;
    for (const ActionState &action : m_actions)
    {
        PersistedBinding binding;
        binding.key = action.key;
        binding.joystickDescriptor = action.joystickBinding.descriptor;
        bindingsToWrite[action.name] = binding;
    }

    for (const auto &bindingEntry : bindingsToWrite)
    {
        tinyxml2::XMLElement *actionElement = document.NewElement("Action");
        actionElement->SetAttribute("name", bindingEntry.first.c_str());
        actionElement->SetAttribute("scancode", static_cast<int>(bindingEntry.second.key));
        if (!bindingEntry.second.joystickDescriptor.empty())
        {
            actionElement->SetAttribute("joystick", bindingEntry.second.joystickDescriptor.c_str());
        }
        rootElement->InsertEndChild(actionElement);
    }

    const tinyxml2::XMLError saveResult = document.SaveFile(m_bindingFilePath.c_str());
    if (saveResult != tinyxml2::XML_SUCCESS)
    {
        SDL_Log("[input] failed to save bindings to %s", m_bindingFilePath.c_str());
        return false;
    }

    return true;
}

SDL_Scancode InputManager::ResolveRegisteredKey(const std::string &name, SDL_Scancode fallbackKey) const
{
    auto loadedIt = m_loadedBindings.find(name);
    if (loadedIt != m_loadedBindings.end())
    {
        return loadedIt->second.key;
    }

    return fallbackKey;
}

std::string InputManager::ResolveRegisteredJoystickDescriptor(const std::string &name,
                                                              const std::string &fallbackDescriptor) const
{
    auto loadedIt = m_loadedBindings.find(name);
    if (loadedIt != m_loadedBindings.end() && !loadedIt->second.joystickDescriptor.empty())
    {
        return loadedIt->second.joystickDescriptor;
    }

    return fallbackDescriptor;
}
