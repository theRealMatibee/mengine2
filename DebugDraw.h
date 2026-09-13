#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <glm/vec2.hpp>

class SpriteBuffer;
class collisionRect;

// Shared world-space-rect-to-NDC debug rendering helper, usable by any gameplay code (Game,
// Enemy, etc.) that wants to draw a flat-colored collision rect via a SpriteBuffer.
namespace DebugDraw
{
// Projects rect into NDC using the given camera state and appends it to buffer as a
// flat-colored quad. Returns true if the rect is at least partially within the viewport.
bool AppendWorldRect(SpriteBuffer *buffer,
                     SDL_GPUTexture *texture,
                     SDL_GPUSampler *sampler,
                     const collisionRect &rect,
                     const SDL_FColor &color,
                     const glm::vec2 &cameraPan,
                     float cameraZoom,
                     float cameraRotation,
                     int viewportWidth,
                     int viewportHeight);

}
