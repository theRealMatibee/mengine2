#include "pch.hpp"

#include "Utility.h"

#include "CrashMonitor.h"
#include "Registry.h"

bool HasCommandLineFlag(int argc, char *argv[], const char *flagName)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == flagName)
        {
            return true;
        }
    }

    return false;
}

bool PreInitializeEngine(int argc, char *argv[])
{
    // Very first action of all: get persisted settings/user variables available before anything
    // else (including SDL) touches them.
    Registry::Instance().Load();

    // Must run before any other engine subsystem initializes: reports (via a message box) and
    // then re-arms the crash trace left over from the previous session, if any.
    CrashMonitor::Instance().CheckPreviousSessionAndArm();

    // --forcewindowed, if passed on the command line, forces windowed mode AND persists that
    // choice to the registry's "fullscreen" preference (default true) for future launches.
    if (HasCommandLineFlag(argc, argv, "--forcewindowed"))
    {
        Registry::Instance().SetBoolValue("fullscreen", false);
        return true;
    }

    return !Registry::Instance().GetBoolValue("fullscreen", true);
}

