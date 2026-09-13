#include "LegacyImage.h"

#include "BitmapFont.h"
#include "Sprite.h"
#include "SpriteBuffer.h"
#include "TextureManager.h"
#include "Translation.h"
#include "tinyxml2.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>

namespace
{
static constexpr float kEpsilon = 0.0001f;
}

std::string LegacyImageStyleLibrary::MakeKey(const std::string &style, const std::string &design)
{
    return style + "::" + design;
}

void LegacyImageStyleLibrary::RegisterIcon(const std::string &style, const std::string &design, const Region &region)
{
    m_icons[MakeKey(style, design)] = region;
}

void LegacyImageStyleLibrary::RegisterFrame(const std::string &style, const std::string &design, const Frame &frame)
{
    m_frames[MakeKey(style, design)] = frame;
}

void LegacyImageStyleLibrary::RegisterFont(const std::string &fontName, const std::string &bitmapFontXmlPath)
{
    m_fonts[fontName] = bitmapFontXmlPath;
}

const LegacyImageStyleLibrary::Region *LegacyImageStyleLibrary::FindIcon(const std::string &style, const std::string &design) const
{
    const auto it = m_icons.find(MakeKey(style, design));
    return (it == m_icons.end()) ? nullptr : &it->second;
}

const LegacyImageStyleLibrary::Frame *LegacyImageStyleLibrary::FindFrame(const std::string &style, const std::string &design) const
{
    const auto it = m_frames.find(MakeKey(style, design));
    return (it == m_frames.end()) ? nullptr : &it->second;
}

const std::string *LegacyImageStyleLibrary::FindFont(const std::string &fontName) const
{
    const auto it = m_fonts.find(fontName);
    return (it == m_fonts.end()) ? nullptr : &it->second;
}

LegacyImage::LegacyImage(TextureManager *textureManager, ShaderLibrary *shaderLibrary)
    : m_textureManager(textureManager), m_shaderLibrary(shaderLibrary)
{
}

bool LegacyImage::LoadFromXmlString(const std::string &xml, const LegacyImageStyleLibrary &styles)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
    {
        return false;
    }

    const tinyxml2::XMLElement *imageElement = doc.FirstChildElement("Image");
    if (imageElement == nullptr)
    {
        return false;
    }

    return LoadFromXmlElement(imageElement, styles);
}

bool LegacyImage::LoadFromXmlElement(const tinyxml2::XMLElement *imageElement, const LegacyImageStyleLibrary &styles)
{
    if (imageElement == nullptr || m_textureManager == nullptr)
    {
        return false;
    }

    m_segments.clear();
    m_labelImage.reset();
    m_labelFont.reset();
    m_labelScaleToRect = false;
    m_labelRectScale = glm::vec2(1.0f, 1.0f);
    m_labelSourceBounds = SDL_FRect{ 0.0f, 0.0f, 0.0f, 0.0f };
    m_labelHAlign = HAlign::Left;
    m_labelVAlign = VAlign::Top;
    m_localBounds = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_type = Type::None;
    m_hollowFrame = false;
    m_animationFrames.clear();
    m_animationDuration = 0.0f;
    m_animationTime = 0.0f;
    m_isAnimating = false;
    m_currentTuOffsetPixels = 0.0f;
    m_currentTvOffsetPixels = 0.0f;

    ParseBaseAttributes(imageElement);

    const char *typeAttr = imageElement->Attribute("type");
    const std::string type = ToUpperCopy(typeAttr != nullptr ? typeAttr : "");

    bool built = false;
    if (type == "SPRITE")
    {
        built = BuildSpriteFromAttributes(imageElement, false, styles);
        m_type = built ? Type::Sprite : Type::None;
    }
    else if (type == "ICON")
    {
        built = BuildSpriteFromAttributes(imageElement, true, styles);
        m_type = built ? Type::Icon : Type::None;
    }
    else if (type == "FRAME")
    {
        built = BuildFrameFromAttributes(imageElement, styles);
        m_type = built ? Type::Frame : Type::None;
    }
    else if (type == "LABEL")
    {
        built = BuildLabelFromAttributes(imageElement, styles);
        m_type = built ? Type::Label : Type::None;
    }

    if (!built)
    {
        m_type = Type::None;
        return false;
    }

    ResolveAndApplyShader(m_shaderName);
    ParseAnimation(imageElement);
    // Push the parsed base color to segments/label now — Update() only does this once an
    // animation frame runs, so static (non-animated) images need it applied at load time.
    ApplyColor(m_currentColor);
    ApplyCurrentStateToGeometry();
    return true;
}

void LegacyImage::SetViewportSize(int widthPixels, int heightPixels)
{
    m_viewportWidth = std::max(1, widthPixels);
    m_viewportHeight = std::max(1, heightPixels);
    ApplyCurrentStateToGeometry();
}

void LegacyImage::SetPositionPixels(const glm::vec2 &positionPixels)
{
    m_basePositionPixels = positionPixels;
    m_currentPositionPixels = positionPixels;
    ApplyCurrentStateToGeometry();
}

void LegacyImage::MovePixels(const glm::vec2 &deltaPixels)
{
    m_basePositionPixels += deltaPixels;
    m_currentPositionPixels += deltaPixels;
    ApplyCurrentStateToGeometry();
}

void LegacyImage::SetScale(const glm::vec2 &scale)
{
    m_baseScale = scale;
    m_currentScale = scale;
    ApplyCurrentStateToGeometry();
}

void LegacyImage::SetRotationRadians(float rotationRadians)
{
    m_baseRotationRadians = rotationRadians;
    m_currentRotationRadians = rotationRadians;
    ApplyCurrentStateToGeometry();
}

void LegacyImage::SetColor(const SDL_FColor &color)
{
    m_baseColor = color;
    m_currentColor = color;
    ApplyColor(color);
}

void LegacyImage::ResetAnimation()
{
    m_animationTime = 0.0f;
    m_isAnimating = !m_animationFrames.empty();
    m_currentTuOffsetPixels = 0.0f;
    m_currentTvOffsetPixels = 0.0f;
    m_currentPositionPixels = m_basePositionPixels;
    m_currentScale = m_baseScale;
    m_currentRotationRadians = m_baseRotationRadians;
    m_currentColor = m_baseColor;
    ApplyColor(m_currentColor);
    ApplyCurrentStateToGeometry();
}

void LegacyImage::Update(float deltaSeconds)
{
    if (m_animationFrames.empty() || !m_isAnimating)
    {
        return;
    }

    m_animationTime += std::max(0.0f, deltaSeconds);

    if (m_animationTime >= m_animationDuration && m_animationDuration > kEpsilon)
    {
        if (m_playAnimationOnce)
        {
            m_animationTime = m_animationDuration;
            m_isAnimating = false;
        }
        else
        {
            m_animationTime = std::fmod(m_animationTime, m_animationDuration);
        }
    }

    m_currentPositionPixels = m_basePositionPixels;
    m_currentScale = m_baseScale;
    m_currentRotationRadians = m_baseRotationRadians;
    m_currentColor = m_baseColor;
    m_currentTuOffsetPixels = 0.0f;
    m_currentTvOffsetPixels = 0.0f;

    for (const AnimationFrame &frame : m_animationFrames)
    {
        if (m_animationTime + kEpsilon < frame.timeFrom || m_animationTime - kEpsilon > frame.timeTo)
        {
            continue;
        }

        const float span = std::max(kEpsilon, frame.timeTo - frame.timeFrom);
        const float t = std::clamp((m_animationTime - frame.timeFrom) / span, 0.0f, 1.0f);

        if (frame.hasRotation)
        {
            m_currentRotationRadians = Lerp(frame.rotateFrom, frame.rotateTo, t);
        }
        if (frame.hasScale)
        {
            m_currentScale.x = Lerp(frame.scaleFromX, frame.scaleToX, t);
            m_currentScale.y = Lerp(frame.scaleFromY, frame.scaleToY, t);
        }
        if (frame.hasPosition)
        {
            m_currentPositionPixels.x = Lerp(frame.xFrom, frame.xTo, t);
            m_currentPositionPixels.y = Lerp(frame.yFrom, frame.yTo, t);
        }
        if (frame.hasColor)
        {
            m_currentColor = LerpColor(frame.colorFrom, frame.colorTo, t);
        }
        if (frame.hasTexCoordOffset)
        {
            m_currentTuOffsetPixels = Lerp(frame.tuFrom, frame.tuTo, t);
            m_currentTvOffsetPixels = Lerp(frame.tvFrom, frame.tvTo, t);
        }

        break;
    }

    ApplyColor(m_currentColor);
    ApplyCurrentStateToGeometry();
}

void LegacyImage::RenderToBuffer(SpriteBuffer *buffer) const
{
    if (buffer == nullptr)
    {
        return;
    }

    if (m_labelImage != nullptr)
    {
        m_labelImage->RenderToBuffer(buffer);
    }

    for (const Segment &segment : m_segments)
    {
        if (segment.sprite != nullptr)
        {
            segment.sprite->RenderToBuffer(buffer);
        }
    }
}

void LegacyImage::ApplyShaderOverride(std::shared_ptr<ShaderLibrary::ShaderResource> shaderResource)
{
    for (Segment &segment : m_segments)
    {
        if (segment.sprite != nullptr)
        {
            segment.sprite->SetShaderOverride(shaderResource);
        }
    }

    if (m_labelImage != nullptr)
    {
        m_labelImage->ApplyShaderOverride(shaderResource);
    }
}

void LegacyImage::ResolveAndApplyShader(const std::string &shaderName)
{
    std::shared_ptr<ShaderLibrary::ShaderResource> resource;
    if (!shaderName.empty())
    {
        if (m_shaderLibrary != nullptr)
        {
            resource = m_shaderLibrary->Find(shaderName);
        }
        if (resource == nullptr)
        {
            std::cerr << "[shader] '" << shaderName << "' not found, falling back to the default shader\n";
        }
    }

    ApplyShaderOverride(std::move(resource));
}

bool LegacyImage::BuildGlyphComposite(const std::vector<GlyphSpriteSpec> &glyphs,
                                      const SDL_FRect &localBounds)
{
    m_type = Type::Label;
    m_segments.clear();
    m_labelImage.reset();
    m_labelScaleToRect = false;
    m_labelRectScale = glm::vec2(1.0f, 1.0f);
    m_labelSourceBounds = SDL_FRect{ 0.0f, 0.0f, 0.0f, 0.0f };
    m_localBounds = localBounds;

    float maxRight = 0.0f;
    float maxBottom = 0.0f;
    for (const GlyphSpriteSpec &glyph : glyphs)
    {
        Segment segment{};
        segment.sprite = std::make_unique<Sprite>(m_textureManager);
        if (!segment.sprite->LoadTexture(glyph.texturePath))
        {
            return false;
        }

        segment.left = glyph.left;
        segment.top = glyph.top;
        segment.right = glyph.right;
        segment.bottom = glyph.bottom;
        segment.atlasX = glyph.atlasX;
        segment.atlasY = glyph.atlasY;
        segment.atlasW = glyph.atlasW;
        segment.atlasH = glyph.atlasH;

        maxRight = std::max(maxRight, segment.right);
        maxBottom = std::max(maxBottom, segment.bottom);
        m_segments.push_back(std::move(segment));
    }

    m_widthPixels = static_cast<int>(std::round(std::max(maxRight, localBounds.x + localBounds.w)));
    m_heightPixels = static_cast<int>(std::round(std::max(maxBottom, localBounds.y + localBounds.h)));
    ApplyColor(m_currentColor);
    ApplyCurrentStateToGeometry();
    return true;
}

SDL_FRect LegacyImage::GetLocalBounds() const
{
    if (m_labelImage != nullptr)
    {
        return m_labelImage->GetLocalBounds();
    }

    return m_localBounds;
}

bool LegacyImage::SetLabelText(const std::string &text)
{
    if (m_type != Type::Label || m_labelFont == nullptr)
    {
        return false;
    }

    std::unique_ptr<LegacyImage> newLabelImage = m_labelFont->CreateLegacyImage(m_textureManager, text);
    if (newLabelImage == nullptr)
    {
        return false;
    }

    m_labelImage = std::move(newLabelImage);
    m_labelSourceBounds = m_labelImage->GetLocalBounds();

    if (m_labelScaleToRect && m_widthPixels > 0 && m_heightPixels > 0 &&
        m_labelSourceBounds.w > 0.0f && m_labelSourceBounds.h > 0.0f)
    {
        m_labelRectScale.x = static_cast<float>(m_widthPixels) / m_labelSourceBounds.w;
        m_labelRectScale.y = static_cast<float>(m_heightPixels) / m_labelSourceBounds.h;
    }

    ApplyCurrentStateToGeometry();
    return true;
}

bool LegacyImage::BuildSpriteFromAttributes(const tinyxml2::XMLElement *imageElement,
                                            bool fromIconLookup,
                                            const LegacyImageStyleLibrary &styles)
{
    std::string texturePath;
    int atlasX = 0;
    int atlasY = 0;
    int atlasW = m_widthPixels;
    int atlasH = m_heightPixels;

    if (fromIconLookup)
    {
        const char *style = imageElement->Attribute("style");
        const char *design = imageElement->Attribute("design");
        if (style == nullptr || design == nullptr)
        {
            return false;
        }

        const LegacyImageStyleLibrary::Region *icon = styles.FindIcon(style, design);
        if (icon == nullptr)
        {
            return false;
        }

        texturePath = icon->texturePath;
        atlasX = icon->x;
        atlasY = icon->y;
        atlasW = icon->width;
        atlasH = icon->height;
    }
    else
    {
        const char *texture = imageElement->Attribute("texture");
        if (texture == nullptr)
        {
            return false;
        }

        texturePath = Translation::Instance().Resolve(texture);

        int tu = 0;
        int tv = 0;
        QueryInt(imageElement, "tu", 0, tu);
        QueryInt(imageElement, "tv", 0, tv);
        atlasX = tu;
        atlasY = tv;
    }

    Segment segment{};
    segment.sprite = std::make_unique<Sprite>(m_textureManager);
    if (!segment.sprite->LoadTexture(texturePath))
    {
        return false;
    }

    if (!fromIconLookup && (atlasW <= 0 || atlasH <= 0))
    {
        // Width/height weren't specified in XML: fall back to the texture's native size.
        atlasW = segment.sprite->GetTextureWidth();
        atlasH = segment.sprite->GetTextureHeight();
    }

    if (atlasW <= 0 || atlasH <= 0)
    {
        return false;
    }

    segment.left = 0.0f;
    segment.top = 0.0f;
    segment.right = static_cast<float>(m_widthPixels > 0 ? m_widthPixels : atlasW);
    segment.bottom = static_cast<float>(m_heightPixels > 0 ? m_heightPixels : atlasH);
    segment.atlasX = atlasX;
    segment.atlasY = atlasY;
    segment.atlasW = atlasW;
    segment.atlasH = atlasH;

    m_widthPixels = static_cast<int>(segment.right - segment.left);
    m_heightPixels = static_cast<int>(segment.bottom - segment.top);
    m_localBounds = SDL_FRect{ segment.left,
                               segment.top,
                               segment.right - segment.left,
                               segment.bottom - segment.top };

    m_segments.push_back(std::move(segment));
    return true;
}

bool LegacyImage::BuildFrameFromAttributes(const tinyxml2::XMLElement *imageElement,
                                           const LegacyImageStyleLibrary &styles)
{
    const char *style = imageElement->Attribute("style");
    const char *design = imageElement->Attribute("design");
    if (style == nullptr || design == nullptr)
    {
        return false;
    }

    const LegacyImageStyleLibrary::Frame *frame = styles.FindFrame(style, design);
    if (frame == nullptr || frame->tileWidth <= 0 || frame->tileHeight <= 0)
    {
        return false;
    }

    m_hollowFrame = ParseBool(imageElement->Attribute("hollow"));

    const int imageWidth = std::max(m_widthPixels, frame->tileWidth * 2);
    const int imageHeight = std::max(m_heightPixels, frame->tileHeight * 2);
    m_widthPixels = imageWidth;
    m_heightPixels = imageHeight;

    if (frame->tileWidth == 1 && frame->tileHeight == 1)
    {
        struct Strip
        {
            float left;
            float top;
            float right;
            float bottom;
        };

        std::vector<Strip> strips;
        if (m_hollowFrame)
        {
            strips = {
                { 0.0f, 0.0f, static_cast<float>(imageWidth), 1.0f },
                { 0.0f, static_cast<float>(imageHeight - 1), static_cast<float>(imageWidth), static_cast<float>(imageHeight) },
                { 0.0f, 1.0f, 1.0f, static_cast<float>(std::max(1, imageHeight - 1)) },
                { static_cast<float>(imageWidth - 1), 1.0f, static_cast<float>(imageWidth), static_cast<float>(std::max(1, imageHeight - 1)) }
            };
        }
        else
        {
            strips = {
                { 0.0f, 0.0f, static_cast<float>(imageWidth), static_cast<float>(imageHeight) }
            };
        }

        for (const Strip &strip : strips)
        {
            if (strip.right <= strip.left || strip.bottom <= strip.top)
            {
                continue;
            }

            Segment segment{};
            segment.sprite = std::make_unique<Sprite>(m_textureManager);
            if (!segment.sprite->LoadTexture(frame->texturePath))
            {
                return false;
            }

            segment.left = strip.left;
            segment.top = strip.top;
            segment.right = strip.right;
            segment.bottom = strip.bottom;
            segment.atlasX = frame->x;
            segment.atlasY = frame->y;
            segment.atlasW = 1;
            segment.atlasH = 1;

            m_segments.push_back(std::move(segment));
        }

        m_localBounds = SDL_FRect{ 0.0f,
                                   0.0f,
                                   static_cast<float>(m_widthPixels),
                                   static_cast<float>(m_heightPixels) };

        return !m_segments.empty();
    }

    const float x0 = 0.0f;
    const float x1 = static_cast<float>(frame->tileWidth);
    const float x2 = static_cast<float>(imageWidth - frame->tileWidth);
    const float x3 = static_cast<float>(imageWidth);

    const float y0 = 0.0f;
    const float y1 = static_cast<float>(frame->tileHeight);
    const float y2 = static_cast<float>(imageHeight - frame->tileHeight);
    const float y3 = static_cast<float>(imageHeight);

    struct Cell
    {
        float left;
        float top;
        float right;
        float bottom;
        int atlasX;
        int atlasY;
    };

    const Cell cells[9] = {
        { x0, y0, x1, y1, frame->x, frame->y },
        { x1, y0, x2, y1, frame->x + frame->tileWidth, frame->y },
        { x2, y0, x3, y1, frame->x + frame->tileWidth * 2, frame->y },
        { x0, y1, x1, y2, frame->x, frame->y + frame->tileHeight },
        { x1, y1, x2, y2, frame->x + frame->tileWidth, frame->y + frame->tileHeight },
        { x2, y1, x3, y2, frame->x + frame->tileWidth * 2, frame->y + frame->tileHeight },
        { x0, y2, x1, y3, frame->x, frame->y + frame->tileHeight * 2 },
        { x1, y2, x2, y3, frame->x + frame->tileWidth, frame->y + frame->tileHeight * 2 },
        { x2, y2, x3, y3, frame->x + frame->tileWidth * 2, frame->y + frame->tileHeight * 2 }
    };

    for (int i = 0; i < 9; ++i)
    {
        if (m_hollowFrame && i == 4)
        {
            continue;
        }

        Segment segment{};
        segment.sprite = std::make_unique<Sprite>(m_textureManager);
        if (!segment.sprite->LoadTexture(frame->texturePath))
        {
            return false;
        }

        segment.left = cells[i].left;
        segment.top = cells[i].top;
        segment.right = cells[i].right;
        segment.bottom = cells[i].bottom;
        segment.atlasX = cells[i].atlasX;
        segment.atlasY = cells[i].atlasY;
        segment.atlasW = frame->tileWidth;
        segment.atlasH = frame->tileHeight;

        m_segments.push_back(std::move(segment));
    }

    m_localBounds = SDL_FRect{ 0.0f,
                               0.0f,
                               static_cast<float>(m_widthPixels),
                               static_cast<float>(m_heightPixels) };

    return !m_segments.empty();
}

bool LegacyImage::BuildLabelFromAttributes(const tinyxml2::XMLElement *imageElement,
                                           const LegacyImageStyleLibrary &styles)
{
    const char *fontName = imageElement->Attribute("font");
    const char *text = imageElement->Attribute("text");
    if (fontName == nullptr)
    {
        return false;
    }

    const std::string resolvedText = Translation::Instance().Resolve(text != nullptr ? text : "");

    const std::string *fontPath = styles.FindFont(fontName);
    if (fontPath == nullptr)
    {
        return false;
    }

    BitmapFont bitmapFont;
    if (!bitmapFont.LoadFromXml(*fontPath))
    {
        return false;
    }

    m_labelFont = std::make_shared<BitmapFont>(std::move(bitmapFont));
    m_labelImage = m_labelFont->CreateLegacyImage(m_textureManager, resolvedText);
    if (m_labelImage == nullptr)
    {
        return false;
    }

    m_labelSourceBounds = m_labelImage->GetLocalBounds();
    m_localBounds = m_labelSourceBounds;

    m_labelHAlign = ParseHAlign(imageElement->Attribute("halign"));
    const char *valignAttr = imageElement->Attribute("valign");
    m_labelVAlign = ParseVAlign(valignAttr != nullptr ? valignAttr : imageElement->Attribute("vjustify"));

    const bool scaleToRect =
        ParseBool(imageElement->Attribute("scaledtorect")) ||
        ParseBool(imageElement->Attribute("scaletorect"));

    if (scaleToRect && m_widthPixels > 0 && m_heightPixels > 0 &&
        m_labelSourceBounds.w > 0.0f && m_labelSourceBounds.h > 0.0f)
    {
        m_labelScaleToRect = true;
        m_labelRectScale.x = static_cast<float>(m_widthPixels) / m_labelSourceBounds.w;
        m_labelRectScale.y = static_cast<float>(m_heightPixels) / m_labelSourceBounds.h;
        m_localBounds = SDL_FRect{ 0.0f,
                                   0.0f,
                                   static_cast<float>(m_widthPixels),
                                   static_cast<float>(m_heightPixels) };
    }

    return true;
}

void LegacyImage::ParseBaseAttributes(const tinyxml2::XMLElement *imageElement)
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int xOffset = 0;
    int yOffset = 0;

    QueryInt(imageElement, "x", 0, x);
    QueryInt(imageElement, "y", 0, y);
    QueryInt(imageElement, "width", 0, width);
    QueryInt(imageElement, "height", 0, height);
    QueryInt(imageElement, "xoffset", 0, xOffset);
    QueryInt(imageElement, "yoffset", 0, yOffset);

    m_basePositionPixels = glm::vec2(static_cast<float>(x), static_cast<float>(y));
    m_currentPositionPixels = m_basePositionPixels;
    m_widthPixels = width;
    m_heightPixels = height;
    m_originPixels = glm::vec2(static_cast<float>(xOffset), static_cast<float>(yOffset));

    float rotation = 0.0f;
    float scale = 1.0f;
    float xScale = scale;
    float yScale = scale;
    QueryFloat(imageElement, "rotation", 0.0f, rotation);
    QueryFloat(imageElement, "scale", 1.0f, scale);
    QueryFloat(imageElement, "xscale", scale, xScale);
    QueryFloat(imageElement, "yscale", scale, yScale);

    m_baseRotationRadians = rotation;
    m_currentRotationRadians = rotation;
    m_baseScale = glm::vec2(xScale, yScale);
    m_currentScale = m_baseScale;

    m_baseColor = ParseHexColor(imageElement->Attribute("color"), SDL_FColor{ 1.0f, 1.0f, 1.0f, 1.0f });
    m_currentColor = m_baseColor;

    const char *shaderAttribute = imageElement->Attribute("shader");
    m_shaderName = (shaderAttribute != nullptr) ? shaderAttribute : "";
}

void LegacyImage::ParseAnimation(const tinyxml2::XMLElement *imageElement)
{
    const tinyxml2::XMLElement *animationElement = imageElement->FirstChildElement("Animation");
    if (animationElement == nullptr)
    {
        return;
    }

    m_playAnimationOnce = ParseBool(animationElement->Attribute("playonce"));

    for (const tinyxml2::XMLElement *frameElement = animationElement->FirstChildElement("Frame");
         frameElement != nullptr;
         frameElement = frameElement->NextSiblingElement("Frame"))
    {
        AnimationFrame frame{};

        QueryFloat(frameElement, "timefrom", 0.0f, frame.timeFrom);
        QueryFloat(frameElement, "timeto", frame.timeFrom, frame.timeTo);
        if (frame.timeTo < frame.timeFrom)
        {
            std::swap(frame.timeFrom, frame.timeTo);
        }

        float v = 0.0f;
        if (QueryFloat(frameElement, "rotatefrom", 0.0f, v))
        {
            frame.hasRotation = true;
            frame.rotateFrom = v;
            QueryFloat(frameElement, "rotateto", v, frame.rotateTo);
        }

        float sxFrom = 0.0f;
        float sxTo = 0.0f;
        float syFrom = 0.0f;
        float syTo = 0.0f;
        const bool hasXScale = QueryFloat(frameElement, "xscalefrom", 0.0f, sxFrom) |
                               QueryFloat(frameElement, "xscaleto", 0.0f, sxTo);
        const bool hasYScale = QueryFloat(frameElement, "yscalefrom", 0.0f, syFrom) |
                               QueryFloat(frameElement, "yscaleto", 0.0f, syTo);

        if (hasXScale || hasYScale)
        {
            frame.hasScale = true;
            frame.scaleFromX = hasXScale ? sxFrom : 1.0f;
            frame.scaleToX = hasXScale ? sxTo : frame.scaleFromX;
            frame.scaleFromY = hasYScale ? syFrom : 1.0f;
            frame.scaleToY = hasYScale ? syTo : frame.scaleFromY;
        }
        else
        {
            float uniformFrom = 1.0f;
            float uniformTo = 1.0f;
            if (QueryFloat(frameElement, "scalefrom", 1.0f, uniformFrom) |
                QueryFloat(frameElement, "scaleto", uniformFrom, uniformTo))
            {
                frame.hasScale = true;
                frame.scaleFromX = uniformFrom;
                frame.scaleToX = uniformTo;
                frame.scaleFromY = uniformFrom;
                frame.scaleToY = uniformTo;
            }
        }

        float xFrom = 0.0f;
        float xTo = 0.0f;
        float yFrom = 0.0f;
        float yTo = 0.0f;
        const bool hasX = QueryFloat(frameElement, "xfrom", 0.0f, xFrom) |
                          QueryFloat(frameElement, "xto", 0.0f, xTo);
        const bool hasY = QueryFloat(frameElement, "yfrom", 0.0f, yFrom) |
                          QueryFloat(frameElement, "yto", 0.0f, yTo);
        if (hasX || hasY)
        {
            frame.hasPosition = true;
            frame.xFrom = hasX ? xFrom : m_basePositionPixels.x;
            frame.xTo = hasX ? xTo : frame.xFrom;
            frame.yFrom = hasY ? yFrom : m_basePositionPixels.y;
            frame.yTo = hasY ? yTo : frame.yFrom;
        }

        const char *colorFrom = frameElement->Attribute("colorfrom");
        const char *colorTo = frameElement->Attribute("colorto");
        if (colorFrom != nullptr || colorTo != nullptr)
        {
            frame.hasColor = true;
            frame.colorFrom = ParseHexColor(colorFrom, m_baseColor);
            frame.colorTo = ParseHexColor(colorTo, frame.colorFrom);
        }

        const char *texcoords = frameElement->Attribute("texcoords");
        if (ParseBool(texcoords))
        {
            frame.hasTexCoordOffset = true;
            QueryFloat(frameElement, "tufrom", 0.0f, frame.tuFrom);
            QueryFloat(frameElement, "tuto", frame.tuFrom, frame.tuTo);
            QueryFloat(frameElement, "tvfrom", 0.0f, frame.tvFrom);
            QueryFloat(frameElement, "tvto", frame.tvFrom, frame.tvTo);
        }

        m_animationFrames.push_back(frame);
        m_animationDuration = std::max(m_animationDuration, frame.timeTo);
    }

    m_isAnimating = !m_animationFrames.empty();
}

void LegacyImage::ApplyCurrentStateToGeometry()
{
    if (m_labelImage != nullptr)
    {
        glm::vec2 labelScale = m_currentScale;
        glm::vec2 labelPosition = m_currentPositionPixels;

        if (m_labelScaleToRect)
        {
            labelScale.x *= m_labelRectScale.x;
            labelScale.y *= m_labelRectScale.y;
            labelPosition.x -= m_labelSourceBounds.x * labelScale.x;
            labelPosition.y -= m_labelSourceBounds.y * labelScale.y;
        }
        else
        {
            if (m_widthPixels > 0)
            {
                if (m_labelHAlign == HAlign::Center)
                {
                    labelPosition.x += static_cast<float>(m_widthPixels) * 0.5f -
                                        (m_labelSourceBounds.x + m_labelSourceBounds.w * 0.5f);
                }
                else if (m_labelHAlign == HAlign::Right)
                {
                    labelPosition.x += static_cast<float>(m_widthPixels) -
                                        (m_labelSourceBounds.x + m_labelSourceBounds.w);
                }
            }

            if (m_heightPixels > 0)
            {
                if (m_labelVAlign == VAlign::Center)
                {
                    labelPosition.y += static_cast<float>(m_heightPixels) * 0.5f -
                                        (m_labelSourceBounds.y + m_labelSourceBounds.h * 0.5f);
                }
                else if (m_labelVAlign == VAlign::Bottom)
                {
                    labelPosition.y += static_cast<float>(m_heightPixels) -
                                        (m_labelSourceBounds.y + m_labelSourceBounds.h);
                }
            }
        }

        m_labelImage->SetViewportSize(m_viewportWidth, m_viewportHeight);
        m_labelImage->SetColor(m_currentColor);
        m_labelImage->SetScale(labelScale);
        m_labelImage->SetRotationRadians(m_currentRotationRadians);
        m_labelImage->SetPositionPixels(labelPosition);
    }

    if (m_segments.empty())
    {
        return;
    }

    const float c = std::cos(m_currentRotationRadians);
    const float s = std::sin(m_currentRotationRadians);

    const glm::vec2 pivot = m_currentPositionPixels + m_originPixels;

    auto transformAndProject = [&](float x, float y) {
        const float localX = x - m_originPixels.x;
        const float localY = y - m_originPixels.y;

        const float sx = localX * m_currentScale.x;
        const float sy = localY * m_currentScale.y;

        const float rotatedX = sx * c - sy * s;
        const float rotatedY = sx * s + sy * c;

        const float worldX = pivot.x + rotatedX;
        const float worldY = pivot.y + rotatedY;

        glm::vec2 ndc;
        ndc.x = (worldX / static_cast<float>(m_viewportWidth)) * 2.0f - 1.0f;
        ndc.y = 1.0f - (worldY / static_cast<float>(m_viewportHeight)) * 2.0f;
        return ndc;
    };

    for (Segment &segment : m_segments)
    {
        if (segment.sprite == nullptr)
        {
            continue;
        }

        const int offsetU = static_cast<int>(std::round(m_currentTuOffsetPixels));
        const int offsetV = static_cast<int>(std::round(m_currentTvOffsetPixels));
        segment.sprite->SetTextureRectPixels(segment.atlasX + offsetU,
                                             segment.atlasY + offsetV,
                                             segment.atlasW,
                                             segment.atlasH);

        const glm::vec2 p0 = transformAndProject(segment.left, segment.top);
        const glm::vec2 p1 = transformAndProject(segment.right, segment.top);
        const glm::vec2 p2 = transformAndProject(segment.right, segment.bottom);
        const glm::vec2 p3 = transformAndProject(segment.left, segment.bottom);

        segment.sprite->SetQuadNDCVertices(p0.x, p0.y,
                                           p1.x, p1.y,
                                           p2.x, p2.y,
                                           p3.x, p3.y);
    }
}

void LegacyImage::ApplyColor(const SDL_FColor &color)
{
    if (m_labelImage != nullptr)
    {
        m_labelImage->SetColor(color);
    }

    for (Segment &segment : m_segments)
    {
        if (segment.sprite != nullptr)
        {
            segment.sprite->SetColor(color);
        }
    }
}

bool LegacyImage::QueryInt(const tinyxml2::XMLElement *element, const char *name, int fallback, int &valueOut)
{
    valueOut = fallback;
    if (element == nullptr || name == nullptr)
    {
        return false;
    }

    return element->QueryIntAttribute(name, &valueOut) == tinyxml2::XML_SUCCESS;
}

bool LegacyImage::QueryFloat(const tinyxml2::XMLElement *element, const char *name, float fallback, float &valueOut)
{
    valueOut = fallback;
    if (element == nullptr || name == nullptr)
    {
        return false;
    }

    return element->QueryFloatAttribute(name, &valueOut) == tinyxml2::XML_SUCCESS;
}

bool LegacyImage::ParseBool(const char *text)
{
    if (text == nullptr)
    {
        return false;
    }

    const std::string upper = ToUpperCopy(text);
    return upper == "TRUE" || upper == "T" || upper == "1" || upper == "YES";
}

SDL_FColor LegacyImage::ParseHexColor(const char *text, const SDL_FColor &fallback)
{
    if (text == nullptr)
    {
        return fallback;
    }

    std::string value(text);
    if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0)
    {
        value = value.substr(2);
    }

    if (value.size() != 8)
    {
        return fallback;
    }

    uint32_t packed = 0;
    std::stringstream ss;
    ss << std::hex << value;
    ss >> packed;
    if (ss.fail())
    {
        return fallback;
    }

    const float inv255 = 1.0f / 255.0f;
    SDL_FColor color{};
    color.r = static_cast<float>((packed >> 24) & 0xFFu) * inv255;
    color.g = static_cast<float>((packed >> 16) & 0xFFu) * inv255;
    color.b = static_cast<float>((packed >> 8) & 0xFFu) * inv255;
    color.a = static_cast<float>(packed & 0xFFu) * inv255;
    return color;
}

std::string LegacyImage::ToUpperCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

LegacyImage::HAlign LegacyImage::ParseHAlign(const char *text)
{
    if (text == nullptr)
    {
        return HAlign::Left;
    }

    const std::string upper = ToUpperCopy(text);
    if (upper == "CENTER" || upper == "CENTRE" || upper == "MIDDLE")
    {
        return HAlign::Center;
    }
    if (upper == "RIGHT")
    {
        return HAlign::Right;
    }
    return HAlign::Left;
}

LegacyImage::VAlign LegacyImage::ParseVAlign(const char *text)
{
    if (text == nullptr)
    {
        return VAlign::Top;
    }

    const std::string upper = ToUpperCopy(text);
    if (upper == "CENTER" || upper == "CENTRE" || upper == "MIDDLE")
    {
        return VAlign::Center;
    }
    if (upper == "BOTTOM")
    {
        return VAlign::Bottom;
    }
    return VAlign::Top;
}

float LegacyImage::Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

SDL_FColor LegacyImage::LerpColor(const SDL_FColor &a, const SDL_FColor &b, float t)
{
    SDL_FColor result{};
    result.r = Lerp(a.r, b.r, t);
    result.g = Lerp(a.g, b.g, t);
    result.b = Lerp(a.b, b.b, t);
    result.a = Lerp(a.a, b.a, t);
    return result;
}
