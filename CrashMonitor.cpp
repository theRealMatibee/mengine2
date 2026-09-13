#include "CrashMonitor.h"

#include "Registry.h"

#include <SDL3/SDL_messagebox.h>

namespace
{
constexpr const char *kTraceKey = "trace";
constexpr const char *kLastErrorKey = "lasterror";
constexpr const char *kCleanExitCountKey = "cleanExitCount";
constexpr const char *kCrashExitCountKey = "crashExitCount";
constexpr const char *kStartupCheckpoint = "main::startup";
}

CrashMonitor &CrashMonitor::Instance()
{
    static CrashMonitor instance;
    return instance;
}

void CrashMonitor::CheckPreviousSessionAndArm()
{
    const std::string previousTrace = Registry::Instance().GetStringValue(kTraceKey, "");
    const std::string previousError = Registry::Instance().GetStringValue(kLastErrorKey, "");

    if (!previousTrace.empty() || !previousError.empty())
    {
        // Tallies consecutive unclean exits (reset to 0 below whenever a session loads clean),
        // so callers can react to repeated crashes rather than just a single one.
        const int crashExitCount = Registry::Instance().GetIntValue(kCrashExitCountKey, 0);
        Registry::Instance().SetIntValue(kCrashExitCountKey, crashExitCount + 1);

        std::string message = "The previous session did not shut down cleanly.\n";
        if (!previousTrace.empty())
        {
            message += "\nLast known state: " + previousTrace;
        }
        if (!previousError.empty())
        {
            message += "\nLast recorded error: " + previousError;
        }

        SDL_Log("[crashmonitor] unclean previous session detected (trace='%s', lasterror='%s'), showing message box",
                previousTrace.c_str(), previousError.c_str());
        if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
                                     "Previous session did not exit cleanly",
                                     message.c_str(),
                                     nullptr))
        {
            SDL_Log("[crashmonitor] SDL_ShowSimpleMessageBox failed: %s", SDL_GetError());
        }
    }
    else
    {
        Registry::Instance().SetIntValue(kCrashExitCountKey, 0);
    }

    Registry::Instance().SetStringValue(kLastErrorKey, "");
    RecordCheckpoint(kStartupCheckpoint);
}

void CrashMonitor::RecordCheckpoint(const std::string &checkpoint)
{
    Registry::Instance().SetStringValue(kTraceKey, checkpoint);
    Registry::Instance().Save();
}

void CrashMonitor::RecordLastError(const std::string &message)
{
    Registry::Instance().SetStringValue(kLastErrorKey, message);
    Registry::Instance().Save();
}

void CrashMonitor::MarkCleanExit()
{
    // Read-increment-write, right before the trace checkpoint itself is cleared below.
    const int cleanExitCount = Registry::Instance().GetIntValue(kCleanExitCountKey, 0);
    Registry::Instance().SetIntValue(kCleanExitCountKey, cleanExitCount + 1);

    Registry::Instance().SetStringValue(kTraceKey, "");
    Registry::Instance().Save();
}
