#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class LegacyImage;
class TextureManager;

class BitmapFont
{
public:
    BitmapFont() = default;

    bool LoadFromXml(const std::string &xmlPath);
    int GetLineHeight() const;
    SDL_FRect MeasureText(const std::string &text) const;
    std::unique_ptr<LegacyImage> CreateLegacyImage(TextureManager *textureManager,
                                                    const std::string &text) const;

private:
    struct Glyph
    {
        int id = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int xOffset = 0;
        int yOffset = 0;
        int xAdvance = 0;
        int page = 0;
    };

    struct PlacedGlyph
    {
        const Glyph *glyph = nullptr;
        float penX = 0.0f;
        float penY = 0.0f;
    };

    const Glyph *FindGlyph(int codePoint) const;
    static std::string ResolveRelativePath(const std::string &xmlPath, const std::string &assetPath);
    bool LayoutText(const std::string &text,
                    std::vector<PlacedGlyph> &placedGlyphs,
                    SDL_FRect &outBounds) const;

    int m_lineHeight = 0;
    int m_base = 0;
    int m_scaleWidth = 0;
    int m_scaleHeight = 0;

    std::unordered_map<int, Glyph> m_glyphs;
    std::unordered_map<int, std::string> m_pagePaths;
};