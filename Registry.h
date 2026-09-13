#pragma once

#include <string>
#include <unordered_map>

// Engine-level persisted key/value store for system settings & user variables (bool/int/float/
// string, all stored internally as strings). Backed by a single XML file located via SDL's
// pref-path (SDL_GetPrefPath), loaded once at boot (the very first boot step, right after SDL
// itself is initialized) and flushed on shutdown plus periodically - every Get/Set call checks
// whether 2 seconds have passed since the last save and, if so and something changed, writes
// the file. This keeps values reasonably crash-safe without saving on every single call. All
// reads/writes are logged to the console via SDL_Log.
class Registry
{
public:
    static Registry &Instance();

    // Shared org/app storage folder (via SDL_GetPrefPath, trailing separator included) that any
    // save system - Registry, save games, input bindings, etc. - should resolve its own file(s)
    // under, so they all land in the same place. Empty string if SDL couldn't resolve one.
    // Resolved once and cached.
    static const std::string &GetStorageFolderPath();

    // Resolves the registry file's path via SDL_GetPrefPath and parses it if present. Missing
    // or unparsable file is not an error - the registry just starts empty and the file is
    // created on the next save.
    bool Load();

    // Writes all in-memory values to the registry XML file immediately, bypassing the periodic
    // 2s throttle. Called at shutdown and internally by MaybeAutosave().
    bool Save();

    std::string GetStringValue(const std::string &key, const std::string &defaultValue);
    int GetIntValue(const std::string &key, int defaultValue);
    float GetFloatValue(const std::string &key, float defaultValue);
    bool GetBoolValue(const std::string &key, bool defaultValue);

    void SetStringValue(const std::string &key, const std::string &value);
    void SetIntValue(const std::string &key, int value);
    void SetFloatValue(const std::string &key, float value);
    void SetBoolValue(const std::string &key, bool value);

private:
    Registry() = default;

    // Saves now if a value has changed since the last save and at least 2 seconds have passed
    // since then - called at the end of every Get (when a fallback is stored) and Set.
    void MaybeAutosave();

    std::unordered_map<std::string, std::string> m_values;
    std::string m_filePath;
    Uint64 m_lastSaveTicks = 0;
    bool m_dirty = false;
};
