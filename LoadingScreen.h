#pragma once

#include "GameModule.h"
#include "TextureManager.h"

#include <memory>

class SpriteBuffer;

// Generic engine-level loading screen. Deliberately NOT part of the normal ModuleSystem
// Start/Update/Render flow (it never becomes the "current" module) - instead it's initialized
// once up front by EngineRuntime and driven manually via GameModule::Context's
// begin/pump/endLoadingScreen callbacks from inside another module's OnInit, so a long blocking
// load can still present animated frames between its own checkpoints.
class LoadingScreen final : public GameModule
{
public:
    LoadingScreen();

    // 0..1, clamped; drives the progress bar fill. Spinner animates regardless of progress.
    void SetProgress(float progress01);

protected:
    bool OnInit() override;
    void OnUpdate(float deltaSeconds) override;
    void OnRender(SpriteBuffer *buffer) const override;

private:
    std::shared_ptr<TextureManager::TextureResource> m_solidTexture;
    float m_spinnerAngle = 0.0f;
    float m_progress = 0.0f;
};
