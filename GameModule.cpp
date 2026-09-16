#include "GameModule.h"

#include "CrashMonitor.h"
#include "SpriteBuffer.h"
#include "TextureManager.h"
#include "Translation.h"
#include "tinyxml2.h"

#include <algorithm>
#include <filesystem>

GameModule::GameModule(std::string moduleName, std::string resourceListPath)
    : m_name(std::move(moduleName)),
      m_resourceListPath(std::move(resourceListPath))
{
    CrashMonitor::Instance().RecordCheckpoint(m_name + "::Constructor");
}

GameModule::~GameModule()
{
    CrashMonitor::Instance().RecordCheckpoint(m_name + "::Destructor");
    Shutdown();
}

bool GameModule::Initialize(const Context &context)
{
    // Records this as the current crash-trace checkpoint (and flushes the registry immediately),
    // so a crash during this module's (possibly lengthy) init/lifetime is attributable to it.
    CrashMonitor::Instance().RecordCheckpoint(m_name + "::Initialize");

    if (m_initialized)
    {
        return true;
    }

    m_context = context;
    if (m_context.textureManager == nullptr || m_context.guiManager == nullptr)
    {
        return false;
    }

    if (!LoadResourceList())
    {
        return false;
    }

    if (!OnInit())
    {
        return false;
    }

    m_transitionStateTime = 0.0f;
    m_transitionState = (m_transitionInDuration > 0.0f) ? TransitionState::TransitionIn : TransitionState::Active;

    OnEnter();
    m_initialized = true;
    m_finished = false;
    m_nextModuleName.clear();
    return true;
}

void GameModule::Resume()
{
    if (!m_initialized)
    {
        return;
    }

    CrashMonitor::Instance().RecordCheckpoint(m_name + "::Resume");

    m_transitionStateTime = 0.0f;
    m_transitionState = (m_transitionInDuration > 0.0f) ? TransitionState::TransitionIn : TransitionState::Active;

    OnResume();
}

void GameModule::Shutdown()
{
    // Records this as the current crash-trace checkpoint (and flushes the registry immediately),
    // in case OnShutdown()/teardown below crashes.
    CrashMonitor::Instance().RecordCheckpoint(m_name + "::Shutdown");

    if (!m_initialized)
    {
        return;
    }

    OnShutdown();

    for (const auto &kv : m_songByName)
    {
        if (m_context.soundSystem != nullptr)
        {
            m_context.soundSystem->StopSound(kv.second);
        }
    }

    for (const auto &kv : m_soundByName)
    {
        if (m_context.soundSystem != nullptr)
        {
            m_context.soundSystem->StopSound(kv.second);
        }
    }

    if (m_context.guiManager != nullptr)
    {
        m_context.guiManager->UnregisterCallbacksForOwner(this);
    }

    m_images.clear();
    m_formNames.clear();
    m_soundByName.clear();
    m_songByName.clear();
    m_shadersByName.clear();
    m_context = Context{};
    m_initialized = false;
    m_transitionState = TransitionState::Active;
    m_transitionStateTime = 0.0f;
    m_transitionOutTarget.clear();
}

void GameModule::Update(float deltaSeconds)
{
    if (!m_initialized)
    {
        return;
    }

    AdvanceTransitionState(deltaSeconds);

    for (const auto &kv : m_images)
    {
        if (kv.second != nullptr)
        {
            kv.second->Update(deltaSeconds);
        }
    }

    OnUpdate(deltaSeconds);

    if (m_context.guiManager != nullptr && m_context.guiManager->ConsumeGamepadNavigationFault())
    {
        OnGamepadNavigationFault();
    }
}

void GameModule::Render(SpriteBuffer *buffer) const
{
    if (!m_initialized || buffer == nullptr)
    {
        return;
    }

    int viewportWidth = 960;
    int viewportHeight = 540;
    if (m_context.renderLayout != nullptr)
    {
        viewportWidth = m_context.renderLayout->virtualWidth;
        viewportHeight = m_context.renderLayout->virtualHeight;
    }

    for (const auto &kv : m_images)
    {
        if (kv.second != nullptr && kv.second->IsVisible())
        {
            kv.second->SetViewportSize(viewportWidth, viewportHeight);
            kv.second->RenderToBuffer(buffer);
        }
    }

    OnRender(buffer);
}

void GameModule::ProcessMouse(int x, int y, bool leftButtonDown)
{
    if (!m_initialized || IsGuiInteractionBlocked())
    {
        return;
    }

    if (m_context.guiManager != nullptr)
    {
        m_context.guiManager->ProcessMouse(x, y, leftButtonDown);
    }
}

bool GameModule::IsFinished() const
{
    return m_finished;
}

const std::string &GameModule::GetNextModuleName() const
{
    return m_nextModuleName;
}

const std::string &GameModule::GetName() const
{
    return m_name;
}

LegacyImage *GameModule::GetImage(const std::string &name) const
{
    const auto it = m_images.find(name);
    if (it == m_images.end())
    {
        return nullptr;
    }

    return it->second.get();
}

bool GameModule::ShowForm(const std::string &name)
{
    if (m_context.guiManager == nullptr)
    {
        return false;
    }

    return m_context.guiManager->ShowForm(name);
}

bool GameModule::RegisterButtonCallback(const std::string &formName,
                                        const std::string &controlName,
                                        std::function<void()> callback)
{
    if (m_context.guiManager == nullptr)
    {
        return false;
    }

    return m_context.guiManager->RegisterButtonCallback(formName,
                                                        controlName,
                                                        this,
                                                        std::move(callback));
}

bool GameModule::PlaySound(const std::string &name, int volumePercentOverride)
{
    if (m_context.soundSystem == nullptr)
    {
        return false;
    }

    const auto it = m_soundByName.find(name);
    if (it == m_soundByName.end())
    {
        return false;
    }

    return m_context.soundSystem->PlaySound(it->second, volumePercentOverride);
}

bool GameModule::PlaySong(const std::string &name, int volumePercentOverride)
{
    if (m_context.soundSystem == nullptr)
    {
        return false;
    }

    const auto it = m_songByName.find(name);
    if (it == m_songByName.end())
    {
        return false;
    }

    return m_context.soundSystem->PlayMusic(it->second, volumePercentOverride);
}

void GameModule::SetBrightness(int level)
{
    SpriteBuffer::SetFakeGammaBrightness(level);
}

int GameModule::GetBrightness()
{
    return SpriteBuffer::GetFakeGammaBrightness();
}

bool GameModule::OnInit()
{
    return true;
}

void GameModule::OnEnter()
{
    if (m_context.guiManager != nullptr)
    {
        m_context.guiManager->HideAllForms();
        for (const std::string &formName : m_formNames)
        {
            m_context.guiManager->ShowForm(formName);
        }
    }
}

void GameModule::OnUpdate(float deltaSeconds)
{
    (void) deltaSeconds;
}

void GameModule::OnRender(SpriteBuffer *buffer) const
{
    (void) buffer;
}

void GameModule::OnShutdown()
{
}

void GameModule::OnResume()
{
}

void GameModule::OnGamepadNavigationFault()
{
}

void GameModule::EndModule(std::string nextModuleName)
{
    m_finished = true;
    m_nextModuleName = std::move(nextModuleName);
}

void GameModule::SetUnregisterAfterFinish(bool shouldUnregister)
{
    m_unregisterAfterFinish = shouldUnregister;
}

bool GameModule::ShouldUnregisterAfterFinish() const
{
    return m_unregisterAfterFinish;
}

void GameModule::ResetFlowState()
{
    m_finished = false;
    m_nextModuleName.clear();
}

const GameModule::Context &GameModule::GetContext() const
{
    return m_context;
}

GameModule::TransitionState GameModule::GetTransitionState() const
{
    return m_transitionState;
}

float GameModule::GetTransitionStateTime() const
{
    return m_transitionStateTime;
}

bool GameModule::IsGuiInteractionBlocked() const
{
    return m_transitionState != TransitionState::Active;
}

float GameModule::GetTransitionInDuration() const
{
    return m_transitionInDuration;
}

float GameModule::GetTransitionOutDuration() const
{
    return m_transitionOutDuration;
}

void GameModule::BeginTransitionOut(std::string nextModuleName)
{
    if (m_transitionState == TransitionState::TransitionOut)
    {
        return;
    }

    if (m_transitionOutDuration <= 0.0f)
    {
        EndModule(std::move(nextModuleName));
        return;
    }

    m_transitionOutTarget = std::move(nextModuleName);
    m_transitionState = TransitionState::TransitionOut;
    m_transitionStateTime = 0.0f;
}

void GameModule::AdvanceTransitionState(float deltaSeconds)
{
    if (m_transitionState == TransitionState::TransitionIn)
    {
        m_transitionStateTime += deltaSeconds;
        if (m_transitionStateTime >= m_transitionInDuration)
        {
            m_transitionState = TransitionState::Active;
            m_transitionStateTime = 0.0f;
        }
        return;
    }

    if (m_transitionState == TransitionState::TransitionOut)
    {
        m_transitionStateTime += deltaSeconds;
        if (m_transitionStateTime >= m_transitionOutDuration)
        {
            // Reset before EndModule so a suspended/resumed module doesn't wake up still
            // parked in TransitionOut and immediately re-fire EndModule with a stale target.
            std::string target = std::move(m_transitionOutTarget);
            m_transitionOutTarget.clear();
            m_transitionState = TransitionState::Active;
            m_transitionStateTime = 0.0f;
            EndModule(std::move(target));
        }
    }
}

bool GameModule::LoadResourceList()
{
    m_formNames.clear();
    m_images.clear();
    m_soundByName.clear();
    m_songByName.clear();
    m_shadersByName.clear();
    m_transitionInDuration = 0.0f;
    m_transitionOutDuration = 0.0f;

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(m_resourceListPath.c_str()) != tinyxml2::XML_SUCCESS)
    {
        return false;
    }

    tinyxml2::XMLElement *moduleElement = doc.FirstChildElement("MODULE");
    if (moduleElement == nullptr)
    {
        return false;
    }

    m_resourceListDirectory =
        std::filesystem::path(m_resourceListPath).parent_path().lexically_normal().string();

    if (tinyxml2::XMLElement *shaders = moduleElement->FirstChildElement("Shaders"))
    {
        if (!LoadShaders(shaders))
        {
            return false;
        }
    }

    if (tinyxml2::XMLElement *forms = moduleElement->FirstChildElement("Forms"))
    {
        if (!LoadForms(forms))
        {
            return false;
        }
    }

    tinyxml2::XMLElement *sprites = moduleElement->FirstChildElement("Sprites");
    if (sprites == nullptr)
    {
        sprites = moduleElement->FirstChildElement("LegacyImages");
    }
    if (sprites != nullptr)
    {
        if (!LoadSprites(sprites))
        {
            return false;
        }
    }

    if (tinyxml2::XMLElement *sounds = moduleElement->FirstChildElement("Sounds"))
    {
        if (!LoadSounds(sounds, false))
        {
            return false;
        }
    }

    if (tinyxml2::XMLElement *songs = moduleElement->FirstChildElement("Songs"))
    {
        if (!LoadSounds(songs, true))
        {
            return false;
        }
    }

    if (const tinyxml2::XMLElement *transitions = moduleElement->FirstChildElement("Transitions"))
    {
        ParseTransitions(transitions);
    }

    return true;
}

bool GameModule::LoadForms(tinyxml2::XMLElement *formsElement)
{
    for (tinyxml2::XMLElement *formElement = formsElement->FirstChildElement("Form");
         formElement != nullptr;
         formElement = formElement->NextSiblingElement("Form"))
    {
        if (const char *fileAttribute = formElement->Attribute("file"))
        {
            const std::string resolvedFile = Translation::Instance().Resolve(fileAttribute);
            const std::string formName = LoadFormByExecutablePath(NormalizePath(resolvedFile));
            if (!formName.empty())
            {
                m_formNames.push_back(formName);
            }
            continue;
        }

        if (const char *nameAttribute = formElement->Attribute("name"))
        {
            m_formNames.emplace_back(nameAttribute);
        }
    }

    return true;
}

std::string GameModule::LoadFormByExecutablePath(const std::string &path) const
{
    if (m_context.guiManager == nullptr)
    {
        return {};
    }

    const char *basePath = SDL_GetBasePath();
    const std::filesystem::path resolved = (basePath != nullptr && basePath[0] != '\0')
        ? (std::filesystem::path(basePath) / path).lexically_normal()
        : std::filesystem::absolute(path).lexically_normal();

    return m_context.guiManager->LoadFormByPath(resolved.string());
}

bool GameModule::LoadSprites(tinyxml2::XMLElement *spritesElement)
{
    for (tinyxml2::XMLElement *spriteElement = spritesElement->FirstChildElement();
         spriteElement != nullptr;
         spriteElement = spriteElement->NextSiblingElement())
    {
        SDL_PumpEvents(); // texture decode per sprite can add up; keep the window responsive
        const char *nameAttribute = spriteElement->Attribute("name");
        if (nameAttribute == nullptr)
        {
            continue;
        }

        std::unique_ptr<LegacyImage> image = std::make_unique<LegacyImage>(m_context.textureManager, m_context.shaderLibrary);

        LegacyImageStyleLibrary emptyStyles;
        if (image->LoadFromXmlElement(spriteElement, emptyStyles))
        {
            m_images[nameAttribute] = std::move(image);
        }
    }
    return true;
}


bool GameModule::LoadShaders(tinyxml2::XMLElement *shadersElement)
{
    if (m_context.shaderLibrary == nullptr)
    {
        return true;
    }

    for (tinyxml2::XMLElement *entry = shadersElement->FirstChildElement();
         entry != nullptr;
         entry = entry->NextSiblingElement())
    {
        const char *nameAttribute = entry->Attribute("name");
        const char *fileAttribute = entry->Attribute("file");
        if (nameAttribute == nullptr || fileAttribute == nullptr)
        {
            continue;
        }

        std::shared_ptr<ShaderLibrary::ShaderResource> resource =
            m_context.shaderLibrary->LoadShader(nameAttribute, Translation::Instance().Resolve(fileAttribute));
        if (resource != nullptr)
        {
            m_shadersByName[nameAttribute] = std::move(resource);
        }
    }

    return true;
}

bool GameModule::LoadSounds(tinyxml2::XMLElement *soundsElement, bool asSongs)
{
    if (m_context.soundSystem == nullptr)
    {
        return true;
    }

    for (tinyxml2::XMLElement *entry = soundsElement->FirstChildElement();
         entry != nullptr;
         entry = entry->NextSiblingElement())
    {
        const char *fileAttribute = entry->Attribute("file");
        if (fileAttribute == nullptr)
        {
            continue;
        }

        const std::string translatedFile = Translation::Instance().Resolve(fileAttribute);

        std::string resourceName;
        if (const char *nameAttribute = entry->Attribute("name"))
        {
            resourceName = nameAttribute;
        }
        else
        {
            resourceName = std::filesystem::path(NormalizePath(translatedFile)).stem().string();
        }

        if (resourceName.empty())
        {
            continue;
        }

        SoundSystem::LoadOptions options{};
        options.looping = asSongs;

        int volume = 100;
        entry->QueryIntAttribute("volume", &volume);
        options.volumePercent = std::clamp(volume, 0, 100);

        const std::string resolvedPath = ResolveResourcePath(translatedFile);
        if (!m_context.soundSystem->LoadSound(resourceName, resolvedPath, options))
        {
            continue;
        }

        if (asSongs)
        {
            m_songByName[resourceName] = resourceName;
        }
        else
        {
            m_soundByName[resourceName] = resourceName;
        }
    }

    return true;
}

void GameModule::ParseTransitions(const tinyxml2::XMLElement *transitionsElement)
{
    float transitionIn = 0.0f;
    float transitionOut = 0.0f;
    transitionsElement->QueryFloatAttribute("in", &transitionIn);
    transitionsElement->QueryFloatAttribute("out", &transitionOut);
    m_transitionInDuration = std::max(0.0f, transitionIn);
    m_transitionOutDuration = std::max(0.0f, transitionOut);
}

std::string GameModule::ResolveResourcePath(const std::string &path) const
{
    const std::string normalized = NormalizePath(path);
    if (normalized.empty())
    {
        return normalized;
    }

    const std::filesystem::path p(normalized);
    if (p.is_absolute())
    {
        return p.lexically_normal().string();
    }

    return (std::filesystem::path(m_resourceListDirectory) / normalized).lexically_normal().string();
}

std::string GameModule::NormalizePath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}
