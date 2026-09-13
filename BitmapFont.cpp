#include "BitmapFont.h"

#include "LegacyImage.h"
#include "TextureManager.h"
#include "Translation.h"
#include "tinyxml2.h"

#include <algorithm>
#include <filesystem>

namespace
{
std::string ResolveFontPagePath(const std::string &xmlPath, const std::string &assetPath)
{
    const std::filesystem::path xmlDirectory = std::filesystem::path(xmlPath).parent_path();

    const std::filesystem::path asset(assetPath);
    std::filesystem::path resolved = asset.is_absolute() ? asset : (xmlDirectory / asset);
    resolved = resolved.lexically_normal();

    if (std::filesystem::exists(resolved))
    {
        return resolved.string();
    }

    // Some BMFont exports leave page file as "Unnamed.png" even when the atlas
    // in the folder matches the font XML base name. Try that as a fallback.
    std::filesystem::path xmlStemCandidate = xmlDirectory / (std::filesystem::path(xmlPath).stem().string() + resolved.extension().string());
    xmlStemCandidate = xmlStemCandidate.lexically_normal();
    if (std::filesystem::exists(xmlStemCandidate))
    {
        return xmlStemCandidate.string();
    }

    return resolved.string();
}

bool DecodeNextUtf8CodePoint(const std::string &text, size_t &index, int &codePoint)
{
    if (index >= text.size())
    {
        return false;
    }

    const unsigned char firstByte = static_cast<unsigned char>(text[index]);

    if ((firstByte & 0x80u) == 0)
    {
        codePoint = static_cast<int>(firstByte);
        index += 1;
        return true;
    }

    int expectedLength = 0;
    int decoded = 0;
    int minimumCodePoint = 0;

    if ((firstByte & 0xE0u) == 0xC0u)
    {
        expectedLength = 2;
        decoded = static_cast<int>(firstByte & 0x1Fu);
        minimumCodePoint = 0x80;
    }
    else if ((firstByte & 0xF0u) == 0xE0u)
    {
        expectedLength = 3;
        decoded = static_cast<int>(firstByte & 0x0Fu);
        minimumCodePoint = 0x800;
    }
    else if ((firstByte & 0xF8u) == 0xF0u)
    {
        expectedLength = 4;
        decoded = static_cast<int>(firstByte & 0x07u);
        minimumCodePoint = 0x10000;
    }
    else
    {
        codePoint = '?';
        index += 1;
        return true;
    }

    if (index + static_cast<size_t>(expectedLength) > text.size())
    {
        codePoint = '?';
        index = text.size();
        return true;
    }

    for (int offset = 1; offset < expectedLength; ++offset)
    {
        const unsigned char continuationByte = static_cast<unsigned char>(text[index + static_cast<size_t>(offset)]);
        if ((continuationByte & 0xC0u) != 0x80u)
        {
            codePoint = '?';
            index += 1;
            return true;
        }

        decoded = (decoded << 6) | static_cast<int>(continuationByte & 0x3Fu);
    }

    if (decoded < minimumCodePoint || decoded > 0x10FFFF ||
        (decoded >= 0xD800 && decoded <= 0xDFFF))
    {
        codePoint = '?';
        index += static_cast<size_t>(expectedLength);
        return true;
    }

    codePoint = decoded;
    index += static_cast<size_t>(expectedLength);
    return true;
}
}

bool BitmapFont::LoadFromXml(const std::string &xmlPath)
{
    tinyxml2::XMLDocument document;
    if (document.LoadFile(xmlPath.c_str()) != tinyxml2::XML_SUCCESS)
    {
        return false;
    }

    const tinyxml2::XMLElement *fontElement = document.FirstChildElement("font");
    if (fontElement == nullptr)
    {
        return false;
    }

    const tinyxml2::XMLElement *commonElement = fontElement->FirstChildElement("common");
    const tinyxml2::XMLElement *pagesElement = fontElement->FirstChildElement("pages");
    const tinyxml2::XMLElement *charsElement = fontElement->FirstChildElement("chars");
    if (commonElement == nullptr || pagesElement == nullptr || charsElement == nullptr)
    {
        return false;
    }

    int lineHeight = 0;
    int base = 0;
    int scaleWidth = 0;
    int scaleHeight = 0;
    if (commonElement->QueryIntAttribute("lineHeight", &lineHeight) != tinyxml2::XML_SUCCESS ||
        commonElement->QueryIntAttribute("base", &base) != tinyxml2::XML_SUCCESS ||
        commonElement->QueryIntAttribute("scaleW", &scaleWidth) != tinyxml2::XML_SUCCESS ||
        commonElement->QueryIntAttribute("scaleH", &scaleHeight) != tinyxml2::XML_SUCCESS)
    {
        return false;
    }

    std::unordered_map<int, std::string> pagePaths;
    for (const tinyxml2::XMLElement *pageElement = pagesElement->FirstChildElement("page");
         pageElement != nullptr;
         pageElement = pageElement->NextSiblingElement("page"))
    {
        int id = 0;
        const char *file = pageElement->Attribute("file");
        if (pageElement->QueryIntAttribute("id", &id) != tinyxml2::XML_SUCCESS || file == nullptr)
        {
            return false;
        }

        pagePaths[id] = ResolveFontPagePath(xmlPath, Translation::Instance().Resolve(file));
    }

    if (pagePaths.empty())
    {
        return false;
    }

    std::unordered_map<int, Glyph> glyphs;
    for (const tinyxml2::XMLElement *charElement = charsElement->FirstChildElement("char");
         charElement != nullptr;
         charElement = charElement->NextSiblingElement("char"))
    {
        Glyph glyph;
        if (charElement->QueryIntAttribute("id", &glyph.id) != tinyxml2::XML_SUCCESS ||
            charElement->QueryIntAttribute("x", &glyph.x) != tinyxml2::XML_SUCCESS ||
            charElement->QueryIntAttribute("y", &glyph.y) != tinyxml2::XML_SUCCESS ||
            charElement->QueryIntAttribute("width", &glyph.width) != tinyxml2::XML_SUCCESS ||
            charElement->QueryIntAttribute("height", &glyph.height) != tinyxml2::XML_SUCCESS ||
            charElement->QueryIntAttribute("xoffset", &glyph.xOffset) != tinyxml2::XML_SUCCESS ||
            charElement->QueryIntAttribute("yoffset", &glyph.yOffset) != tinyxml2::XML_SUCCESS ||
            charElement->QueryIntAttribute("xadvance", &glyph.xAdvance) != tinyxml2::XML_SUCCESS)
        {
            return false;
        }

        charElement->QueryIntAttribute("page", &glyph.page);
        glyphs[glyph.id] = glyph;
    }

    m_lineHeight = lineHeight;
    m_base = base;
    m_scaleWidth = scaleWidth;
    m_scaleHeight = scaleHeight;
    m_pagePaths = std::move(pagePaths);
    m_glyphs = std::move(glyphs);
    return true;
}

int BitmapFont::GetLineHeight() const
{
    return m_lineHeight;
}

SDL_FRect BitmapFont::MeasureText(const std::string &text) const
{
    SDL_FRect bounds{};
    std::vector<PlacedGlyph> placedGlyphs;
    if (!LayoutText(text, placedGlyphs, bounds))
    {
        return SDL_FRect{ 0.0f, 0.0f, 0.0f, 0.0f };
    }

    return bounds;
}

std::unique_ptr<LegacyImage> BitmapFont::CreateLegacyImage(TextureManager *textureManager,
                                                            const std::string &text) const
{
    if (textureManager == nullptr)
    {
        return nullptr;
    }

    std::vector<PlacedGlyph> placedGlyphs;
    SDL_FRect bounds{};
    if (!LayoutText(text, placedGlyphs, bounds))
    {
        return nullptr;
    }

    std::vector<LegacyImage::GlyphSpriteSpec> glyphSpecs;
    glyphSpecs.reserve(placedGlyphs.size());

    for (const PlacedGlyph &placedGlyph : placedGlyphs)
    {
        if (placedGlyph.glyph == nullptr)
        {
            continue;
        }

        const Glyph &glyph = *placedGlyph.glyph;
        const auto pageIt = m_pagePaths.find(glyph.page);
        if (pageIt == m_pagePaths.end())
        {
            return nullptr;
        }

        LegacyImage::GlyphSpriteSpec spec{};
        spec.texturePath = pageIt->second;
        spec.atlasX = glyph.x;
        spec.atlasY = glyph.y;
        spec.atlasW = glyph.width;
        spec.atlasH = glyph.height;
        spec.left = placedGlyph.penX + static_cast<float>(glyph.xOffset);
        spec.top = placedGlyph.penY + static_cast<float>(glyph.yOffset);
        spec.right = spec.left + static_cast<float>(glyph.width);
        spec.bottom = spec.top + static_cast<float>(glyph.height);
        glyphSpecs.push_back(std::move(spec));
    }

    auto legacyImage = std::make_unique<LegacyImage>(textureManager);
    if (!legacyImage->BuildGlyphComposite(glyphSpecs, bounds))
    {
        return nullptr;
    }

    return legacyImage;
}

const BitmapFont::Glyph *BitmapFont::FindGlyph(int codePoint) const
{
    const auto it = m_glyphs.find(codePoint);
    if (it != m_glyphs.end())
    {
        return &it->second;
    }

    const auto fallbackIt = m_glyphs.find(static_cast<int>('?'));
    if (fallbackIt != m_glyphs.end())
    {
        return &fallbackIt->second;
    }

    return nullptr;
}

std::string BitmapFont::ResolveRelativePath(const std::string &xmlPath, const std::string &assetPath)
{
    return ResolveFontPagePath(xmlPath, assetPath);
}

bool BitmapFont::LayoutText(const std::string &text,
                            std::vector<PlacedGlyph> &placedGlyphs,
                            SDL_FRect &outBounds) const
{
    placedGlyphs.clear();
    outBounds = SDL_FRect{ 0.0f, 0.0f, 0.0f, 0.0f };

    if (m_pagePaths.empty())
    {
        return false;
    }

    float penX = 0.0f;
    float penY = 0.0f;
    bool haveBounds = false;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    size_t index = 0;
    while (index < text.size())
    {
        int codePoint = 0;
        if (!DecodeNextUtf8CodePoint(text, index, codePoint))
        {
            break;
        }

        if (codePoint == '\r')
        {
            continue;
        }

        if (codePoint == '\n')
        {
            penX = 0.0f;
            penY += static_cast<float>(m_lineHeight);
            continue;
        }

        const Glyph *glyph = FindGlyph(codePoint);
        if (glyph == nullptr)
        {
            continue;
        }

        if (glyph->width <= 0 || glyph->height <= 0)
        {
            penX += static_cast<float>(glyph->xAdvance);
            continue;
        }

        const auto pageIt = m_pagePaths.find(glyph->page);
        if (pageIt == m_pagePaths.end())
        {
            return false;
        }

        const float glyphLeft = penX + static_cast<float>(glyph->xOffset);
        const float glyphTop = penY + static_cast<float>(glyph->yOffset);
        const float glyphRight = glyphLeft + static_cast<float>(glyph->width);
        const float glyphBottom = glyphTop + static_cast<float>(glyph->height);

        if (!haveBounds)
        {
            minX = glyphLeft;
            minY = glyphTop;
            maxX = glyphRight;
            maxY = glyphBottom;
            haveBounds = true;
        }
        else
        {
            minX = std::min(minX, glyphLeft);
            minY = std::min(minY, glyphTop);
            maxX = std::max(maxX, glyphRight);
            maxY = std::max(maxY, glyphBottom);
        }

        PlacedGlyph placedGlyph{};
        placedGlyph.glyph = glyph;
        placedGlyph.penX = penX;
        placedGlyph.penY = penY;
        placedGlyphs.push_back(placedGlyph);

        penX += static_cast<float>(glyph->xAdvance);
    }

    if (haveBounds)
    {
        outBounds.x = minX;
        outBounds.y = minY;
        outBounds.w = maxX - minX;
        outBounds.h = maxY - minY;
    }

    return true;
}
