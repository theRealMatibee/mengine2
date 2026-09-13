#pragma once

#include <optional>

#include "GameModule.h"

struct SdlBootstrapContext;
class ModuleSystem;
class GUIManager;

GameModule::RenderLayout ComputeRenderLayout(int windowWidth, int windowHeight);
// Updates the virtual resolution used by ComputeRenderLayout for subsequent frames.
void SetVirtualResolution(int width, int height);
// Hides the OS mouse cursor immediately, regardless of fullscreen state, and arms the main
// loop's existing motion-based watchdog (see RunMainLoop) so it reappears automatically the
// next time the user moves the mouse - any module can call this to request a hidden cursor
// without needing to track/restore visibility itself.
void RequestCursorAutoHide();
int RunMainLoop(SdlBootstrapContext &sdl,
				ModuleSystem &moduleSystem,
				GUIManager &guiManager,
				GameModule::RenderLayout &renderLayout,
				bool guiLoaded);

// Renders and presents a single frame for one module directly, bypassing ModuleSystem/the main
// loop's input+timing bookkeeping entirely. Used to manually pump the engine-level loading
// screen from inside another module's (potentially multi-second, blocking) OnInit - see
// GameModule::Context::pumpLoadingScreen. Safe to call repeatedly in a tight loop; each call is
// one full acquire/render/present cycle.
void PresentGameModuleFrame(SdlBootstrapContext &sdl,
                            GameModule *module,
                            const GameModule::RenderLayout &renderLayout);



void UpdateWindowTitle(SDL_Window *window, std::string& title, std::optional<bool> showFps);

const float GetStableFps();