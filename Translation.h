#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// Engine-level string/asset localisation. LoadFromFile() parses every <Language name="..."">
// region of a locale XML file (each containing <string name="@@key" value="..."/> entries) and
// keeps all of them permanently in memory - intended to be called once at app startup. Any
// filename or string literal decoded while loading engine resources (textures, sounds, shaders,
// GUI form/label text, etc.) should be passed through Resolve(). LoadFromFile() also picks the
// active language once loading is done: the Registry's "override_language" value wins if set,
// otherwise the languages loaded from the XML are matched against the OS's preferred locales.
class Translation
{
public:
    static Translation &Instance();

    // Parses every <Language> element directly under the file's root and merges their strings
    // into memory (safe to call more than once, e.g. to layer additional locale files), then
    // selects the active language (see class comment). Returns true if at least one
    // <string name="..." value="..."/> entry was loaded.
    bool LoadFromFile(const std::string &path);

    // Selects which loaded language Resolve() reads from. Returns false (leaving the active
    // language unchanged) if languageName was never loaded.
    bool SetActiveLanguage(const std::string &languageName);
    const std::string &GetActiveLanguage() const;
    std::vector<std::string> GetAvailableLanguages() const;

    // Looks up value in the active language's string table when value starts with "@@" and a
    // matching <string name="value"> entry exists; otherwise returns value unchanged/intact.
    std::string Resolve(const std::string &value) const;

    // Same lookup as Resolve(), but reads languageName's string table instead of the active
    // language - for UI that needs to preview/display text in a language other than the one
    // currently in effect (e.g. a language-picker showing each option's own help text).
    // languageName never being loaded is treated the same as a missing key (value returned as-is).
    std::string ResolveInLanguage(const std::string &languageName, const std::string &value) const;

private:
    Translation() = default;

    // Matches the OS's preferred locales (in order) against m_languageOrder; falls back to "en"
    // if none match or SDL has no preferred locales.
    std::string DetermineStartupLanguage() const;

    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_stringsByLanguage;
    std::vector<std::string> m_languageOrder;
    std::string m_activeLanguage;
};
