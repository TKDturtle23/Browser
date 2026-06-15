#pragma once

#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <string>
#include <unordered_map>
#include <vector>
class IRenderBackend;

struct Glyph {
    int width;
    int height;
    int bearingX;
    int bearingY;
    int advance;
    std::vector<uint8_t> bitmap; // Used directly by Software Backend

    // Unified texture coordinates
    float u0, v0;
    float u1, v1;

    // Cross-platform resource identifier
    // 0 means no texture (software fallback or whitespace)
    uint32_t textureAssetID = 0;
};
struct FontMetrics {
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    int lineHeight = 0;
};
#include <string>
#include <string_view>
#include <stdexcept>
#include <cstdint>

std::u32string Utf8ToUtf32(std::string_view input);

class Font {
public:
    Font(const std::string& path, int pixelSize);
    ~Font();



    const Glyph& GetGlyph(IRenderBackend* backend, char32_t c) const;
    int GetLineHeight() const;
    FT_Vector GetKerning(char32_t c, char32_t prev_char) const;

    FontMetrics GetMetrics() const; // REQUIRED
    void SetSize(IRenderBackend* backend, int pixelSize);
    int GetCurrentSize() const { return currentSize; }

    bool HasSymbol(char32_t c) const;

private:
    int currentSize;
    FT_Library ft;
    FT_Face face;

    mutable std::unordered_map<char32_t, Glyph> cache;

    mutable uint32_t fontTextureID = 0; // The generic abstract ID token!
    mutable int atlasX = 0;
    mutable int atlasY = 0;
    mutable int maxRowHeight = 0;
    const int ATLAS_SIZE = 512;
    void LoadGlyph(IRenderBackend* backend, char32_t c) const;

};
struct FallbackFonts {
    Font& Primary;
    Font& Symbol;
    Font& Emoji;
};