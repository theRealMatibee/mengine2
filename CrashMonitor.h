#pragma once

#include <string>

// Lightweight unclean-shutdown detector layered on top of Registry's persisted key/value store.
// Deliberately NOT a logging system: only the single most recent lifecycle checkpoint ("trace")
// and the single most recent recorded error ("lasterror") are kept, each overwritten in place
// rather than appended to, so this stays a cheap breadcrumb rather than a growing log file.
//
// Every write here calls Registry::Save() immediately (bypassing its normal 2s autosave
// throttle) so it survives a crash occurring right after the call returns - this only stays
// cheap because writes are limited to rare, coarse-grained checkpoints (module lifecycle
// transitions), never per-frame.
class CrashMonitor
{
public:
    static CrashMonitor &Instance();

    // Must be called as early as possible - before any other engine subsystem initializes -
    // right after Registry::Instance().Load(). If a "trace" and/or "lasterror" value survived
    // from the previous run, that means MarkCleanExit() was never reached last session (crash,
    // force-kill, hang); reports it via SDL_ShowSimpleMessageBox (safe to call before SDL_Init())
    // so the user/developer sees roughly where things went wrong, and increments the persisted
    // "crashExitCount" tally (reset to 0 instead when the previous session loaded clean) so
    // callers can react to repeated consecutive crashes rather than just a single one. Always
    // re-arms "trace"/"lasterror" for the current session afterwards, regardless of what (if
    // anything) was found.
    void CheckPreviousSessionAndArm();

    // Records a coarse lifecycle checkpoint (e.g. "Game::Initialize") as the current "trace".
    // Intended for infrequent module-lifecycle transitions only (Initialize/Resume/Shutdown) -
    // never per-frame.
    void RecordCheckpoint(const std::string &checkpoint);

    // Records a one-off error message for the next session's CheckPreviousSessionAndArm() to
    // report, separate from the routine checkpoint trail.
    void RecordLastError(const std::string &message);

    // Marks the current session as having exited via the message pump's normal quit path -
    // increments the persisted "cleanExitCount" tally, then clears "trace" so the next startup
    // sees a clean previous session.
    void MarkCleanExit();

private:
    CrashMonitor() = default;
};
