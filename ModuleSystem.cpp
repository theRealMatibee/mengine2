#include "ModuleSystem.h"

#include "SpriteBuffer.h"

#include <algorithm>
#include <utility>

ModuleSystem::~ModuleSystem()
{
    Stop();
}

void ModuleSystem::SetContext(const GameModule::Context &context)
{
    m_context = context;
}

void ModuleSystem::RegisterModule(ModuleType type, std::unique_ptr<GameModule> module)
{
    if (!IsValidType(type))
    {
        return;
    }

    const int index = ToIndex(type);
    if (index < 0)
    {
        return;
    }

    if (m_modules[static_cast<size_t>(index)] != nullptr)
    {
        m_modules[static_cast<size_t>(index)]->Shutdown();
    }

    m_modules[static_cast<size_t>(index)] = std::move(module);
}

void ModuleSystem::UnregisterModule(ModuleType type)
{
    if (!IsValidType(type))
    {
        return;
    }

    const int index = ToIndex(type);
    if (index < 0)
    {
        return;
    }

    if (m_currentType == type)
    {
        Stop();
    }

    if (m_modules[static_cast<size_t>(index)] != nullptr)
    {
        m_modules[static_cast<size_t>(index)]->Shutdown();
    }

    m_modules[static_cast<size_t>(index)].reset();
    m_suspendedStack.erase(std::remove(m_suspendedStack.begin(), m_suspendedStack.end(), type),
                          m_suspendedStack.end());
}

GameModule *ModuleSystem::GetRegisteredModule(ModuleType type) const
{
    if (!IsValidType(type))
    {
        return nullptr;
    }

    const int index = ToIndex(type);
    if (index < 0)
    {
        return nullptr;
    }

    return m_modules[static_cast<size_t>(index)].get();
}

bool ModuleSystem::Start(ModuleType moduleType)
{
    return Activate(moduleType);
}

bool ModuleSystem::StartAuto()
{
    const ModuleType first = FindFirstRegistered();
    if (!IsValidType(first))
    {
        return false;
    }

    return Activate(first);
}

void ModuleSystem::Stop()
{
    GameModule *current = GetCurrentModule();
    if (current != nullptr)
    {
        current->Shutdown();
    }

    for (const ModuleType suspendedType : m_suspendedStack)
    {
        if (suspendedType == m_currentType)
        {
            continue;
        }

        const int suspendedIndex = ToIndex(suspendedType);
        if (suspendedIndex >= 0)
        {
            GameModule *suspended = m_modules[static_cast<size_t>(suspendedIndex)].get();
            if (suspended != nullptr)
            {
                suspended->Shutdown();
            }
        }
    }

    m_currentType = ModuleType::Count;
    m_suspendedStack.clear();
}

void ModuleSystem::Update(float deltaSeconds)
{
    GameModule *current = GetCurrentModule();
    if (current == nullptr)
    {
        return;
    }

    m_inputManager.Update();
    current->Update(deltaSeconds);

    if (m_context.soundSystem != nullptr)
    {
        m_context.soundSystem->Update(deltaSeconds);
    }

    if (current->IsFinished())
    {
        const ModuleType finishedType = m_currentType;
        const bool unregisterFinished = current->ShouldUnregisterAfterFinish();
        const std::string nextName = current->GetNextModuleName();

        ModuleType nextType = ModuleType::Count;
        if (!nextName.empty())
        {
            nextType = FindTypeByModuleName(nextName);
        }
        else
        {
            nextType = FindNextRegisteredAfter(finishedType);
        }

        const bool topOfStackMatches =
            !m_suspendedStack.empty() && m_suspendedStack.back() == nextType;
        const bool resumeFromOverlay = IsOverlayType(finishedType) && topOfStackMatches;
        const bool suspendForOverlay =
            IsOverlayType(nextType) && nextType != finishedType && IsValidType(nextType);

        if (resumeFromOverlay)
        {
            current->Shutdown();
            m_currentType = ModuleType::Count;

            const ModuleType resumeType = m_suspendedStack.back();
            m_suspendedStack.pop_back();

            if (!Activate(resumeType))
            {
                Stop();
            }
            else
            {
                GameModule *resumed = GetCurrentModule();
                if (resumed != nullptr)
                {
                    resumed->Resume();
                }
            }
            return;
        }

        if (suspendForOverlay)
        {
            current->ResetFlowState();
            m_suspendedStack.push_back(finishedType);
            m_currentType = ModuleType::Count;

            if (!Activate(nextType))
            {
                Stop();
            }
            return;
        }

        current->Shutdown();
        if (unregisterFinished)
        {
            const int finishedIndex = ToIndex(finishedType);
            if (finishedIndex >= 0)
            {
                m_modules[static_cast<size_t>(finishedIndex)].reset();
            }
        }

        m_currentType = ModuleType::Count;

        if (IsOverlayType(finishedType) && !m_suspendedStack.empty())
        {
            // Leaving the overlay chain entirely (e.g. quitting to the main menu):
            // shut down every module still suspended beneath it.
            for (auto it = m_suspendedStack.rbegin(); it != m_suspendedStack.rend(); ++it)
            {
                const int suspendedIndex = ToIndex(*it);
                if (suspendedIndex >= 0)
                {
                    GameModule *suspended = m_modules[static_cast<size_t>(suspendedIndex)].get();
                    if (suspended != nullptr)
                    {
                        suspended->Shutdown();
                    }
                }
            }
            m_suspendedStack.clear();
        }

        if (!IsValidType(nextType) || !Activate(nextType))
        {
            Stop();
        }
    }
}

void ModuleSystem::ProcessMouse(int x, int y, bool leftButtonDown)
{
    GameModule *current = GetCurrentModule();
    if (current != nullptr)
    {
        current->ProcessMouse(x, y, leftButtonDown);
    }
}

void ModuleSystem::Render(SpriteBuffer *buffer) const
{
    GameModule *current = GetCurrentModule();
    if (current != nullptr)
    {
        current->Render(buffer);
    }
}

GameModule *ModuleSystem::GetCurrentModule() const
{
    if (!IsValidType(m_currentType))
    {
        return nullptr;
    }

    const int index = ToIndex(m_currentType);
    if (index < 0)
    {
        return nullptr;
    }

    return m_modules[static_cast<size_t>(index)].get();
}

ModuleSystem::ModuleType ModuleSystem::GetCurrentModuleType() const
{
    return m_currentType;
}

bool ModuleSystem::IsValidType(ModuleType type)
{
    return type >= ModuleType::Splash && type < ModuleType::Count;
}

bool ModuleSystem::IsOverlayType(ModuleType type)
{
    return type == ModuleType::Pause || type == ModuleType::OptionsMenu || type == ModuleType::SaveSlots;
}

int ModuleSystem::ToIndex(ModuleType type)
{
    if (!IsValidType(type))
    {
        return -1;
    }

    return static_cast<int>(type);
}

const char *ModuleSystem::ToDebugName(ModuleType type)
{
    switch (type)
    {
        case ModuleType::Splash:
            return "Splash";
        case ModuleType::Intro:
            return "Intro";
        case ModuleType::MainMenu:
            return "MainMenu";
        case ModuleType::Game:
            return "Game";
        case ModuleType::Pause:
            return "Pause";
        case ModuleType::Outro:
            return "Outro";
        case ModuleType::OptionsMenu:
            return "OptionsMenu";
        case ModuleType::SaveSlots:
            return "SaveSlots";
        case ModuleType::LoadingScreen:
            return "LoadingScreen";
        default:
            return "Invalid";
    }
}

bool ModuleSystem::Activate(ModuleType moduleType)
{
    if (!IsValidType(moduleType))
    {
        return false;
    }

    const int nextIndex = ToIndex(moduleType);
    if (nextIndex < 0)
    {
        return false;
    }

    GameModule *next = m_modules[static_cast<size_t>(nextIndex)].get();
    if (next == nullptr)
    {
        return false;
    }

    if (!next->Initialize(m_context))
    {
        return false;
    }

    m_currentType = moduleType;
    return true;
}

ModuleSystem::ModuleType ModuleSystem::FindFirstRegistered() const
{
    for (int i = 0; i < static_cast<int>(ModuleType::Count); ++i)
    {
        if (m_modules[static_cast<size_t>(i)] != nullptr)
        {
            return static_cast<ModuleType>(i);
        }
    }

    return ModuleType::Count;
}

ModuleSystem::ModuleType ModuleSystem::FindNextRegisteredAfter(ModuleType moduleType) const
{
    if (!IsValidType(moduleType))
    {
        return ModuleType::Count;
    }

    const int start = ToIndex(moduleType) + 1;
    for (int i = start; i < static_cast<int>(ModuleType::Count); ++i)
    {
        if (m_modules[static_cast<size_t>(i)] != nullptr)
        {
            return static_cast<ModuleType>(i);
        }
    }

    return ModuleType::Count;
}

ModuleSystem::ModuleType ModuleSystem::FindTypeByModuleName(const std::string &moduleName) const
{
    if (moduleName.empty())
    {
        return ModuleType::Count;
    }

    for (int i = 0; i < static_cast<int>(ModuleType::Count); ++i)
    {
        GameModule *module = m_modules[static_cast<size_t>(i)].get();
        if (module != nullptr && module->GetName() == moduleName)
        {
            return static_cast<ModuleType>(i);
        }
    }

    return ModuleType::Count;
}
