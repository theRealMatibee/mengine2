#pragma once

#include <functional>

struct SdlBootstrapContext;
class ModuleSystem;

// Returned by RunEngineRuntime() when a module requested a soft reset (GameModule::Context's
// requestSoftReset()) rather than a real quit - the caller should immediately invoke
// RunEngineRuntime() again instead of shutting down.
constexpr int kSoftResetExitCode = 2;

int RunEngineRuntime(SdlBootstrapContext &sdl,
					 const std::function<void(ModuleSystem &)> &registerModules);
