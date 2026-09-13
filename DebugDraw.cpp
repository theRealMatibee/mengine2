#include "DebugDraw.h"

#include "2dCollisions.hpp"
#include "SpriteBuffer.h"

#include <algorithm>
#include <cmath>

bool DebugDraw::AppendWorldRect(SpriteBuffer *buffer,
                                SDL_GPUTexture *texture,
                                SDL_GPUSampler *sampler,
                                const collisionRect &rect,
                                const SDL_FColor &color,
                                const glm::vec2 &cameraPan,
                                float cameraZoom,
                                float cameraRotation,
                                int viewportWidth,
                                int viewportHeight)
{
    if (buffer == nullptr || texture == nullptr || viewportWidth <= 0 || viewportHeight <= 0)
    {
        return false;
    }

    const float c = std::cos(-cameraRotation);
    const float s = std::sin(-cameraRotation);
    const float halfViewportWidth = static_cast<float>(viewportWidth) * 0.5f;
    const float halfViewportHeight = static_cast<float>(viewportHeight) * 0.5f;

    auto toNdc = [&](const glm::vec2 &worldPoint) -> glm::vec2
    {
        const float dx = worldPoint.x - cameraPan.x;
        const float dy = worldPoint.y - cameraPan.y;
        const float cameraX = dx * c - dy * s;
        const float cameraY = dx * s + dy * c;
        return glm::vec2((cameraX * cameraZoom) / halfViewportWidth,
                         -(cameraY * cameraZoom) / halfViewportHeight);
    };

    const glm::vec2 topLeft = toNdc(rect.topLeft());
    const glm::vec2 topRight = toNdc(glm::vec2(rect.bottomRight().x, rect.topLeft().y));
    const glm::vec2 bottomRight = toNdc(rect.bottomRight());
    const glm::vec2 bottomLeft = toNdc(glm::vec2(rect.topLeft().x, rect.bottomRight().y));

    const float minX = std::min(std::min(topLeft.x, topRight.x), std::min(bottomLeft.x, bottomRight.x));
    const float maxX = std::max(std::max(topLeft.x, topRight.x), std::max(bottomLeft.x, bottomRight.x));
    const float minY = std::min(std::min(topLeft.y, topRight.y), std::min(bottomLeft.y, bottomRight.y));
    const float maxY = std::max(std::max(topLeft.y, topRight.y), std::max(bottomLeft.y, bottomRight.y));
    const bool intersectsViewport = !(maxX < -1.0f || minX > 1.0f || maxY < -1.0f || minY > 1.0f);

    SpriteBuffer::Vertex vertices[4] = {
        { topLeft.x, topLeft.y, 0.0f, 0.0f, 0.0f, color.r, color.g, color.b, color.a },
        { topRight.x, topRight.y, 0.0f, 1.0f, 0.0f, color.r, color.g, color.b, color.a },
        { bottomRight.x, bottomRight.y, 0.0f, 1.0f, 1.0f, color.r, color.g, color.b, color.a },
        { bottomLeft.x, bottomLeft.y, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b, color.a }
    };

    buffer->AddSprite(vertices, texture, sampler);
    return intersectsViewport;
}

