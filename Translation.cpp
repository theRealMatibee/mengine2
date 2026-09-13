#include "Translation.h"

#include "Registry.h"
#include "tinyxml2.h"

#include <algorithm>

Translation &Translation::Instance()
{
    static Translation instance;
    return instance;
}

bool Translation::LoadFromFile(const std::string &path)
{
    tinyxml2::XMLDocument document;
    if (document.LoadFile(path.c_str()) != tinyxml2::XML_SUCCESS)
    {
        return false;
    }

    const tinyxml2::XMLElement *root = document.RootElement();
    if (root == nullptr)
    {
        return false;
    }

    bool loadedAny = false;
    for (const tinyxml2::XMLElement *languageElement = root->FirstChildElement("Language");
         languageElement != nullptr;
         languageElement = languageElement->NextSiblingElement("Language"))
    {
        const char *languageName = languageElement->Attribute("name");
        if (languageName == nullptr || languageName[0] == '\0')
        {
            continue;
        }

        std::unordered_map<std::string, std::string> &strings = m_stringsByLanguage[languageName];
        if (std::find(m_languageOrder.begin(), m_languageOrder.end(), languageName) == m_languageOrder.end())
        {
            m_languageOrder.push_back(languageName);
        }

        for (const tinyxml2::XMLElement *stringElement = languageElement->FirstChildElement("string");
             stringElement != nullptr;
             stringElement = stringElement->NextSiblingElement("string"))
        {
            const char *name = stringElement->Attribute("name");
            const char *value = stringElement->Attribute("value");
            if (name == nullptr || value == nullptr)
            {
                continue;
            }

            strings[name] = value;
            loadedAny = true;
        }
    }

    if (!m_languageOrder.empty())
    {
        const std::string overrideLanguage = Registry::Instance().GetStringValue("override_language", "no");
        const std::string selectedLanguage = (overrideLanguage == "no")
            ? DetermineStartupLanguage()
            : Registry::Instance().GetStringValue("language", "en");

        if (!SetActiveLanguage(selectedLanguage))
        {
            SetActiveLanguage(m_languageOrder.front());
        }
    }

    return loadedAny;
}

std::string Translation::DetermineStartupLanguage() const
{
    int count = 0;
    SDL_Locale **locales = SDL_GetPreferredLocales(&count);

    if (locales != nullptr)
    {
        // Loop through the user's OS-preferred locales in order and match against the languages
        // actually loaded from the locale XML.
        for (int i = 0; locales[i] != nullptr; i++)
        {
            const char *userLang = locales[i]->language;
            for (const std::string &supportedLang : m_languageOrder)
            {
                if (SDL_strcmp(userLang, supportedLang.c_str()) == 0)
                {
                    const std::string matched = supportedLang;
                    SDL_free(locales);
                    return matched;
                }
            }
        }
        SDL_free(locales);
    }

    return "en";
}

bool Translation::SetActiveLanguage(const std::string &languageName)
{
    if (m_stringsByLanguage.find(languageName) == m_stringsByLanguage.end())
    {
        return false;
    }

    m_activeLanguage = languageName;
    return true;
}

const std::string &Translation::GetActiveLanguage() const
{
    return m_activeLanguage;
}

std::vector<std::string> Translation::GetAvailableLanguages() const
{
    return m_languageOrder;
}

std::string Translation::Resolve(const std::string &value) const
{
    return ResolveInLanguage(m_activeLanguage, value);
}

std::string Translation::ResolveInLanguage(const std::string &languageName, const std::string &value) const
{
    if (value.rfind("@@", 0) != 0)
    {
        return value;
    }

    const auto languageIt = m_stringsByLanguage.find(languageName);
    if (languageIt == m_stringsByLanguage.end())
    {
        return value;
    }

    const auto stringIt = languageIt->second.find(value);
    if (stringIt == languageIt->second.end())
    {
        return value;
    }

    return stringIt->second;
}
