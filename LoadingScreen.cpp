#include "LoadingScreen.h"

#include "2dCollisions.hpp"
#include "DebugDraw.h"
#include "SpriteBuffer.h"

#include <algorithm>

namespace
{
constexpr float kSpinnerRadiansPerSecond = 3.0f;
constexpr float kSpinnerHalfSize = 40.0f;
constexpr float kProgressBarWidth = 600.0f;
constexpr float kProgressBarHeight = 24.0f;
}

LoadingScreen::LoadingScreen()
    : GameModule("LoadingScreen", "assets/LoadingScreen/resources.xml")
{
}

bool LoadingScreen::OnInit()
{
    if (GetContext().textureManager == nullptr)
    {
        return false;
    }

    m_solidTexture = GetContext().textureManager->CreateSolidColorTexture(SDL_FColor{ 1.0f, 1.0f, 1.0f, 1.0f });
    return m_solidTexture != nullptr;
}

void LoadingScreen::SetProgress(float progress01)
{
    m_progress = std::clamp(progress01, 0.0f, 1.0f);
}

void LoadingScreen::OnUpdate(float deltaSeconds)
{
    m_spinnerAngle += deltaSeconds * kSpinnerRadiansPerSecond;
}

void LoadingScreen::OnRender(SpriteBuffer *buffer) const
{
    if (buffer == nullptr || m_solidTexture == nullptr)
    {
        return;
    }

    int viewportWidth = 960;
    int viewportHeight = 540;
    if (const RenderLayout *layout = GetContext().renderLayout)
    {
        viewportWidth = layout->virtualWidth;
        viewportHeight = layout->virtualHeight;
    }

    // Screen-space overlay: treat the viewport center as the shared pivot for every rect below
    // rather than a real game camera (this module has none).
    const glm::vec2 screenCenter(static_cast<float>(viewportWidth) * 0.5f, static_cast<float>(viewportHeight) * 0.5f);

    auto charPlot = []( SpriteBuffer *buffer, 
                        std::shared_ptr<TextureManager::TextureResource> solidTexture, 
                        glm::vec2 screenCenter, 
                        int *pixels, 
                        float x, float y, 
                        float width, float height, 
                        float hgap, float vgap,
                        float logoRotation,
                        SDL_FColor color, 
                        int viewportWidth, int viewportHeight)
    {
        for ( int i = 0; i < 8; ++i )
        {
            float xOffset = x;
            int pixel = pixels[i];
            for ( int t = 7; t >= 0; --t )
            {
                if ( pixel & (1 << t))
                {
                    DebugDraw::AppendWorldRect(buffer,
                              solidTexture->texture,
                              solidTexture->sampler,
                              collisionRect(glm::vec2(xOffset, y), glm::vec2(xOffset + width, y + height)),
                              color,
                              screenCenter,
                              1.0f,
                              logoRotation,
                              viewportWidth,
                              viewportHeight);
                }
                xOffset += width + hgap;
            }
            y += height + vgap;
        }
    };

    float logoRot = 0.1f;

    int pixels_m[8] = { 0,0,102,127,127,107,99,0 };
    charPlot(buffer, m_solidTexture, screenCenter, pixels_m, 800.0f, 300.0f, 70.0f, 70.0f, 4.0, 6.0f, logoRot, SDL_FColor{ 0.0f, 1.0f, 0.0f, 0.3f }, viewportWidth, viewportHeight);

    int pixels_2[8] = { 0, 124, 102, 12, 24, 48, 126, 0 };
    charPlot(buffer, m_solidTexture, screenCenter, pixels_2, 1150.0f, 260.0f, 40.0f, 40.0f, 4.0, 6.0f, logoRot, SDL_FColor{ 1.0f, 0.0f, 0.0f, 0.3f }, viewportWidth, viewportHeight);
    
    float x = 650.0f; float chsize=16.0f; float pitch=chsize*8.0f; 
    float y = 760.0f;
    // 'L' (ATASCII 76)
    int pixels_L[8] = {0,96,96,96,96,96,126,0};
    charPlot(buffer, m_solidTexture, screenCenter, pixels_L, x, y, chsize, chsize, 0.0, 0.0f, logoRot, SDL_FColor{ 1.0f, 1.0f, 1.0f, 0.3f }, viewportWidth, viewportHeight);
    x+=pitch;

    // 'o' (ATASCII 111)
    int pixels_o[8] = {0,0,60,102,102,102,60,0};
    charPlot(buffer, m_solidTexture, screenCenter, pixels_o, x, y, chsize, chsize, 0.0, 0.0f, logoRot, SDL_FColor{ 1.0f, 1.0f, 1.0f, 0.3f }, viewportWidth, viewportHeight);
    x+=pitch;

    // 'a' (ATASCII 97)
    int pixels_a[8] = {0,0,60,6,62,102,62,0};
    charPlot(buffer, m_solidTexture, screenCenter, pixels_a, x, y, chsize, chsize, 0.0, 0.0f, logoRot, SDL_FColor{ 1.0f, 1.0f, 1.0f, 0.3f }, viewportWidth, viewportHeight);
    x+=pitch;

    // 'd' (ATASCII 100)
    int pixels_d[8] = {0,6,6,62,102,102,62,0};
    charPlot(buffer, m_solidTexture, screenCenter, pixels_d, x, y, chsize, chsize, 0.0, 0.0f, logoRot, SDL_FColor{ 1.0f, 1.0f, 1.0f, 0.3f }, viewportWidth, viewportHeight);
    x+=pitch;

    // 'i' (ATASCII 105)
    int pixels_i[8] = {0,24,0,56,24,24,60,0};
    charPlot(buffer, m_solidTexture, screenCenter, pixels_i, x, y, chsize, chsize, 0.0, 0.0f, logoRot, SDL_FColor{ 1.0f, 1.0f, 1.0f, 0.3f }, viewportWidth, viewportHeight);
    x+=pitch;

    // 'n' (ATASCII 110)
    int pixels_n[8] = {0,0,124,102,102,102,102,0};
    charPlot(buffer, m_solidTexture, screenCenter, pixels_n, x, y, chsize, chsize, 0.0, 0.0f, logoRot, SDL_FColor{ 1.0f, 1.0f, 1.0f, 0.3f }, viewportWidth, viewportHeight);
    x+=pitch;

    // 'g' (ATASCII 103)
    int pixels_g[8] = {0,0,62,102,102,62,6,124};
    charPlot(buffer, m_solidTexture, screenCenter, pixels_g, x, y, chsize, chsize, 0.0, 0.0f, logoRot, SDL_FColor{ 1.0f, 1.0f, 1.0f, 0.3f }, viewportWidth, viewportHeight);
    x+=pitch;

    // '.' (ATASCII 46)
    int pixels_dot[8] = {0,0,0,0,0,24,24,0};
    charPlot(buffer, m_solidTexture, screenCenter, pixels_dot, x, y, chsize, chsize, 0.0, 0.0f, logoRot, SDL_FColor{ 1.0f, 1.0f, 1.0f, 0.3f }, viewportWidth, viewportHeight);
    x+=pitch;
    charPlot(buffer, m_solidTexture, screenCenter, pixels_dot, x, y, chsize, chsize, 0.0, 0.0f, logoRot, SDL_FColor{ 1.0f, 1.0f, 1.0f, 0.3f }, viewportWidth, viewportHeight);
    x+=pitch;

    const collisionRect spinnerRect(screenCenter - glm::vec2(kSpinnerHalfSize),
                                    screenCenter + glm::vec2(kSpinnerHalfSize));
    DebugDraw::AppendWorldRect(buffer,
                              m_solidTexture->texture,
                              m_solidTexture->sampler,
                              spinnerRect,
                              SDL_FColor{ 0.9f, 0.9f, 0.95f, 0.95f },
                              screenCenter,
                              1.0f,
                              m_spinnerAngle,
                              viewportWidth,
                              viewportHeight);

    const glm::vec2 barCenter(screenCenter.x, screenCenter.y + kSpinnerHalfSize * 2.5f);
    const collisionRect barBackground(barCenter - glm::vec2(kProgressBarWidth * 0.5f, kProgressBarHeight * 0.5f),
                                      barCenter + glm::vec2(kProgressBarWidth * 0.5f, kProgressBarHeight * 0.5f));
    DebugDraw::AppendWorldRect(buffer,
                              m_solidTexture->texture,
                              m_solidTexture->sampler,
                              barBackground,
                              SDL_FColor{ 0.2f, 0.2f, 0.2f, 0.66f },
                              screenCenter,
                              1.0f,
                              0.0f,
                              viewportWidth,
                              viewportHeight);

    if (m_progress > 0.0f)
    {
        const float fillWidth = kProgressBarWidth * m_progress;
        const glm::vec2 fillTopLeft = barBackground.topLeft();
        const collisionRect barFill(fillTopLeft, fillTopLeft + glm::vec2(fillWidth, kProgressBarHeight));
        DebugDraw::AppendWorldRect(buffer,
                                  m_solidTexture->texture,
                                  m_solidTexture->sampler,
                                  barFill,
                                  SDL_FColor{ 0.3f, 0.7f, 0.95f, 1.0f },
                                  screenCenter,
                                  1.0f,
                                  0.0f,
                                  viewportWidth,
                                  viewportHeight);
    }
}
