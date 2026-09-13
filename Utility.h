#pragma once

// Returns true if flagName (e.g. "--forcewindowed") appears verbatim as one of argv[1..argc-1] -
// only bare/exact flags are matched, no "name=value" parsing.
bool HasCommandLineFlag(int argc, char *argv[], const char *flagName);

// The earliest engine-level startup steps, in order: loads the persisted Registry (must happen
// before anything else, including SDL, touches it), runs CrashMonitor's previous-session
// check/report (must also run before any other engine subsystem initializes), then resolves the
// windowed-vs-fullscreen decision - the registry's "fullscreen" preference (default true) unless
// "--forcewindowed" is present on the command line, which also persists that choice for future
// launches. Returns whether the app should start in windowed mode.
bool PreInitializeEngine(int argc, char *argv[]);
