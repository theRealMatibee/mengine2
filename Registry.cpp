#include "pch.hpp"

#include "Registry.h"

#include "tinyxml2.h"

namespace
{
constexpr const char *kRegistryOrgName = "sdl3-new";
constexpr const char *kRegistryAppName = "sdl3-new";
constexpr const char *kRegistryFileName = "registry.xml";
constexpr Uint64 kAutosaveIntervalMs = 2000;
}

Registry &Registry::Instance()
{
    static Registry instance;
    return instance;
}

const std::string &Registry::GetStorageFolderPath()
{
    static const std::string storageFolderPath = []() -> std::string
    {
        char *prefPath = SDL_GetPrefPath(kRegistryOrgName, kRegistryAppName);
        if (prefPath == nullptr || prefPath[0] == '\0')
        {
            if (prefPath != nullptr)
            {
                SDL_free(prefPath);
            }
            SDL_Log("[registry] could not resolve a shared storage folder");
            return std::string();
        }

        std::string resolvedPath(prefPath);
        SDL_free(prefPath);
        return resolvedPath;
    }();

    return storageFolderPath;
}

bool Registry::Load()
{
    const std::string &storageFolderPath = GetStorageFolderPath();
    if (storageFolderPath.empty())
    {
        SDL_Log("[registry] could not resolve a pref path, registry will not persist");
        return false;
    }

    m_filePath = storageFolderPath + kRegistryFileName;

    // Starts the 2s autosave window from boot rather than from ticks-since-epoch-zero, so the
    // first Get/Set call doesn't immediately trigger a save.
    m_lastSaveTicks = SDL_GetTicks();

    tinyxml2::XMLDocument document;
    if (document.LoadFile(m_filePath.c_str()) != tinyxml2::XML_SUCCESS)
    {
        SDL_Log("[registry] no existing registry file at '%s', starting empty", m_filePath.c_str());
        return false;
    }

    const tinyxml2::XMLElement *root = document.FirstChildElement("Registry");
    if (root == nullptr)
    {
        SDL_Log("[registry] '%s' has no <Registry> root element, starting empty", m_filePath.c_str());
        return false;
    }

    for (const tinyxml2::XMLElement *valueElement = root->FirstChildElement("Value");
         valueElement != nullptr;
         valueElement = valueElement->NextSiblingElement("Value"))
    {
        const char *key = valueElement->Attribute("key");
        const char *value = valueElement->Attribute("value");
        if (key == nullptr || value == nullptr)
        {
            continue;
        }

        m_values[key] = value;
    }

    SDL_Log("[registry] loaded %zu value(s) from '%s'", m_values.size(), m_filePath.c_str());
    return true;
}

bool Registry::Save()
{
    m_lastSaveTicks = SDL_GetTicks();
    m_dirty = false;

    if (m_filePath.empty())
    {
        SDL_Log("[registry] no file path resolved, skipping save");
        return false;
    }

    tinyxml2::XMLDocument document;
    tinyxml2::XMLElement *root = document.NewElement("Registry");
    document.InsertEndChild(root);

    for (const auto &entry : m_values)
    {
        tinyxml2::XMLElement *valueElement = document.NewElement("Value");
        valueElement->SetAttribute("key", entry.first.c_str());
        valueElement->SetAttribute("value", entry.second.c_str());
        root->InsertEndChild(valueElement);
    }

    const tinyxml2::XMLError saveResult = document.SaveFile(m_filePath.c_str());
    if (saveResult != tinyxml2::XML_SUCCESS)
    {
        SDL_Log("[registry] failed to save '%s' (tinyxml2 error %d)",
                m_filePath.c_str(), static_cast<int>(saveResult));
        return false;
    }

    SDL_Log("[registry] saved %zu value(s) to '%s'", m_values.size(), m_filePath.c_str());
    return true;
}

void Registry::MaybeAutosave()
{
    if (!m_dirty)
    {
        return;
    }

    if (SDL_GetTicks() - m_lastSaveTicks >= kAutosaveIntervalMs)
    {
        Save();
    }
}

std::string Registry::GetStringValue(const std::string &key, const std::string &defaultValue)
{
    const auto it = m_values.find(key);
    if (it == m_values.end())
    {
        SDL_Log("[registry] read '%s' -> not found, using default '%s'", key.c_str(), defaultValue.c_str());
        SetStringValue(key, defaultValue);
        return defaultValue;
    }

    SDL_Log("[registry] read '%s' -> '%s'", key.c_str(), it->second.c_str());
    MaybeAutosave();
    return it->second;
}

int Registry::GetIntValue(const std::string &key, int defaultValue)
{
    const std::string stringValue = GetStringValue(key, std::to_string(defaultValue));
    try
    {
        return std::stoi(stringValue);
    }
    catch (...)
    {
        SDL_Log("[registry] '%s' value '%s' is not a valid int, using default %d",
                key.c_str(), stringValue.c_str(), defaultValue);
        return defaultValue;
    }
}

float Registry::GetFloatValue(const std::string &key, float defaultValue)
{
    const std::string stringValue = GetStringValue(key, std::to_string(defaultValue));
    try
    {
        return std::stof(stringValue);
    }
    catch (...)
    {
        SDL_Log("[registry] '%s' value '%s' is not a valid float, using default %f",
                key.c_str(), stringValue.c_str(), defaultValue);
        return defaultValue;
    }
}

bool Registry::GetBoolValue(const std::string &key, bool defaultValue)
{
    const std::string stringValue = GetStringValue(key, defaultValue ? "true" : "false");
    return stringValue == "true" || stringValue == "1";
}

void Registry::SetStringValue(const std::string &key, const std::string &value)
{
    m_values[key] = value;
    m_dirty = true;
    SDL_Log("[registry] write '%s' -> '%s'", key.c_str(), value.c_str());
    MaybeAutosave();
}

void Registry::SetIntValue(const std::string &key, int value)
{
    SetStringValue(key, std::to_string(value));
}

void Registry::SetFloatValue(const std::string &key, float value)
{
    SetStringValue(key, std::to_string(value));
}

void Registry::SetBoolValue(const std::string &key, bool value)
{
    SetStringValue(key, value ? "true" : "false");
}
