#include <SDL3/SDL.h>
#include "2dCollisions.hpp"

static glm::vec2 g_sdlcoll_screenOrigin( 0.0f, 0.0f );
void sdlColl_setScreenOrigin( const glm::vec2& origin );

void drawCollisionRect( SDL_Renderer * renderer, const collisionRect & rect );
void drawCollisionCircle( SDL_Renderer * renderer, const collisionCircle & circle );
void drawCollisionPolygon( SDL_Renderer * renderer, const collisionPolygon & polygon );