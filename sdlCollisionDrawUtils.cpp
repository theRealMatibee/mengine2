#include "sdlCollisionDrawUtils.hpp"

void sdlColl_setScreenOrigin( const glm::vec2& origin ) {
    g_sdlcoll_screenOrigin = origin;
};

void drawCollisionRect( SDL_Renderer * renderer, const collisionRect & rect ) {
    SDL_FRect sdlRect;
    sdlRect.x = static_cast<float>(rect.topLeft().x) - g_sdlcoll_screenOrigin.x;
    sdlRect.y = static_cast<float>(rect.topLeft().y) - g_sdlcoll_screenOrigin.y;
    sdlRect.w = static_cast<float>(rect.bottomRight().x - rect.topLeft().x);
    sdlRect.h = static_cast<float>(rect.bottomRight().y - rect.topLeft().y);
    SDL_RenderRect(renderer, &sdlRect);
};

void drawCollisionCircle( SDL_Renderer * renderer, const collisionCircle & circle ) {
    // Approximate the circle with a polygon
    const int segments = 32;
    SDL_FPoint points[segments + 1];
    for (int i = 0; i < segments; ++i) {
        float angle = (2.0f * M_PI * i) / segments;
        points[i].x = circle.center().x + (circle.radius() * cos(angle)) - g_sdlcoll_screenOrigin.x;
        points[i].y = circle.center().y + (circle.radius() * sin(angle)) - g_sdlcoll_screenOrigin.y;
    }
    points[segments] = points[0]; // Close the circle
    SDL_RenderLines(renderer, points, segments + 1);
};

void drawCollisionPolygon( SDL_Renderer * renderer, const collisionPolygon & polygon ) {
    const std::vector<glm::vec2>& points = polygon.points();
    std::vector<SDL_FPoint> sdlPoints(points.size() + 1);
    for (size_t i = 0; i < points.size(); ++i) {
        sdlPoints[i].x = points[i].x - g_sdlcoll_screenOrigin.x;
        sdlPoints[i].y = points[i].y - g_sdlcoll_screenOrigin.y;
    }
    sdlPoints[points.size()].x = points[0].x - g_sdlcoll_screenOrigin.x; // Close the polygon
    sdlPoints[points.size()].y = points[0].y - g_sdlcoll_screenOrigin.y;
    SDL_RenderLines(renderer, sdlPoints.data(), static_cast<int>(sdlPoints.size()));
};