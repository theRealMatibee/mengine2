#pragma once

#include <SDL3/SDL.h>

#include <glm/vec2.hpp>

#include "Sprite.h"
#include "ShaderLibrary.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class SpriteBuffer;
class TextureManager;
class BitmapFont;
namespace tinyxml2
{
class XMLElement;
}

class LegacyImageStyleLibrary
{
public:
    struct Region
    {
        std::string texturePath;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    struct Frame
    {
        std::string texturePath;
        int x = 0;
        int y = 0;
        int tileWidth = 0;
        int tileHeight = 0;
    };

    void RegisterIcon(const std::string &style, const std::string &design, const Region &region);
    void RegisterFrame(const std::string &style, const std::string &design, const Frame &frame);
    void RegisterFont(const std::string &fontName, const std::string &bitmapFontXmlPath);

    const Region *FindIcon(const std::string &style, const std::string &design) const;
    const Frame *FindFrame(const std::string &style, const std::string &design) const;
    const std::string *FindFont(const std::string &fontName) const;

private:
    static std::string MakeKey(const std::string &style, const std::string &design);

    std::unordered_map<std::string, Region> m_icons;
    std::unordered_map<std::string, Frame> m_frames;
    std::unordered_map<std::string, std::string> m_fonts;
};

class LegacyImage
{
public:
    struct GlyphSpriteSpec
    {
        std::string texturePath;
        int atlasX = 0;
        int atlasY = 0;
        int atlasW = 0;
        int atlasH = 0;
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
    };

    explicit LegacyImage(TextureManager *textureManager, ShaderLibrary *shaderLibrary = nullptr);

    bool LoadFromXmlString(const std::string &xml, const LegacyImageStyleLibrary &styles);
    bool LoadFromXmlElement(const tinyxml2::XMLElement *imageElement, const LegacyImageStyleLibrary &styles);

    void SetViewportSize(int widthPixels, int heightPixels);

    void SetPositionPixels(const glm::vec2 &positionPixels);
    void MovePixels(const glm::vec2 &deltaPixels);
    void SetScale(const glm::vec2 &scale);
    void SetRotationRadians(float rotationRadians);
    void SetColor(const SDL_FColor &color);

    // Lets a module opt an image out of GameModule::Render()'s automatic per-frame draw pass
    // (e.g. a sprite the module repositions/hides itself and renders manually instead).
    void SetVisible(bool visible);
    bool IsVisible() const;

    void ResetAnimation();
    void Update(float deltaSeconds);

    bool BuildGlyphComposite(const std::vector<GlyphSpriteSpec> &glyphs,
                             const SDL_FRect &localBounds);
    SDL_FRect GetLocalBounds() const;
    void RenderToBuffer(SpriteBuffer *buffer) const;

    // Re-lays-out this LABEL image's glyphs for new text using the font it was originally built
    // with, keeping the existing width/height/halign/vjustify/scale rules. Returns false (no-op)
    // if this image isn't a LABEL built from a font.
    bool SetLabelText(const std::string &text);

    // Applies a shader resource (or nullptr for the SpriteBuffer default) to every sprite segment
    // owned by this image, and recursively to any nested label glyph image.
    void ApplyShaderOverride(std::shared_ptr<ShaderLibrary::ShaderResource> shaderResource);

private:
    enum class Type
    {
        None,
        Sprite,
        Icon,
        Frame,
        Label
    };

    enum class HAlign
    {
        Left,
        Center,
        Right
    };

    enum class VAlign
    {
        Top,
        Center,
        Bottom
    };

    struct Segment
    {
        std::unique_ptr<Sprite> sprite;
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
        int atlasX = 0;
        int atlasY = 0;
        int atlasW = 0;
        int atlasH = 0;
    };

    struct AnimationFrame
    {
        float timeFrom = 0.0f;
        float timeTo = 0.0f;

        bool hasRotation = false;
        float rotateFrom = 0.0f;
        float rotateTo = 0.0f;

        bool hasScale = false;
        float scaleFromX = 1.0f;
        float scaleToX = 1.0f;
        float scaleFromY = 1.0f;
        float scaleToY = 1.0f;

        bool hasPosition = false;
        float xFrom = 0.0f;
        float xTo = 0.0f;
        float yFrom = 0.0f;
        float yTo = 0.0f;

        bool hasColor = false;
        SDL_FColor colorFrom = { 1.0f, 1.0f, 1.0f, 1.0f };
        SDL_FColor colorTo = { 1.0f, 1.0f, 1.0f, 1.0f };

        bool hasTexCoordOffset = false;
        float tuFrom = 0.0f;
        float tuTo = 0.0f;
        float tvFrom = 0.0f;
        float tvTo = 0.0f;
    };

    bool BuildSpriteFromAttributes(const tinyxml2::XMLElement *imageElement, bool fromIconLookup, const LegacyImageStyleLibrary &styles);
    bool BuildFrameFromAttributes(const tinyxml2::XMLElement *imageElement, const LegacyImageStyleLibrary &styles);
    bool BuildLabelFromAttributes(const tinyxml2::XMLElement *imageElement, const LegacyImageStyleLibrary &styles);
    void ParseBaseAttributes(const tinyxml2::XMLElement *imageElement);
    void ParseAnimation(const tinyxml2::XMLElement *imageElement);
    void ResolveAndApplyShader(const std::string &shaderName);

    void ApplyCurrentStateToGeometry();
    void ApplyColor(const SDL_FColor &color);

    static bool QueryInt(const tinyxml2::XMLElement *element, const char *name, int fallback, int &valueOut);
    static bool QueryFloat(const tinyxml2::XMLElement *element, const char *name, float fallback, float &valueOut);
    static bool ParseBool(const char *text);
    static SDL_FColor ParseHexColor(const char *text, const SDL_FColor &fallback);
    static std::string ToUpperCopy(std::string value);
    static float Lerp(float a, float b, float t);
    static SDL_FColor LerpColor(const SDL_FColor &a, const SDL_FColor &b, float t);
    static HAlign ParseHAlign(const char *text);
    static VAlign ParseVAlign(const char *text);

    TextureManager *m_textureManager = nullptr;
    ShaderLibrary *m_shaderLibrary = nullptr;
    std::string m_shaderName;

    Type m_type = Type::None;
    std::vector<Segment> m_segments;
    std::unique_ptr<LegacyImage> m_labelImage;
    std::shared_ptr<BitmapFont> m_labelFont;
    bool m_labelScaleToRect = false;
    glm::vec2 m_labelRectScale = glm::vec2(1.0f, 1.0f);
    SDL_FRect m_labelSourceBounds = { 0.0f, 0.0f, 0.0f, 0.0f };
    HAlign m_labelHAlign = HAlign::Left;
    VAlign m_labelVAlign = VAlign::Top;
    SDL_FRect m_localBounds = { 0.0f, 0.0f, 0.0f, 0.0f };

    glm::vec2 m_basePositionPixels = glm::vec2(0.0f, 0.0f);
    glm::vec2 m_currentPositionPixels = glm::vec2(0.0f, 0.0f);
    glm::vec2 m_baseScale = glm::vec2(1.0f, 1.0f);
    glm::vec2 m_currentScale = glm::vec2(1.0f, 1.0f);
    float m_baseRotationRadians = 0.0f;
    float m_currentRotationRadians = 0.0f;
    glm::vec2 m_originPixels = glm::vec2(0.0f, 0.0f);

    int m_widthPixels = 0;
    int m_heightPixels = 0;
    bool m_hollowFrame = false;
    bool m_visible = true;

    SDL_FColor m_baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    SDL_FColor m_currentColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    int m_viewportWidth = 960;
    int m_viewportHeight = 540;

    bool m_playAnimationOnce = false;
    bool m_isAnimating = false;
    float m_animationTime = 0.0f;
    float m_animationDuration = 0.0f;
    float m_currentTuOffsetPixels = 0.0f;
    float m_currentTvOffsetPixels = 0.0f;
    std::vector<AnimationFrame> m_animationFrames;
};
