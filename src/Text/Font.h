#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string>
#include <unordered_map>
#include <vector>
struct Glyph {
    int width;
    int height;
    int bearingX;
    int bearingY;
    int advance;

    std::vector<unsigned char> bitmap;
};
struct FontMetrics {
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    int lineHeight = 0;
};
class Font {
public:
    Font(const std::string& path, int pixelSize);
    ~Font();

    const Glyph& GetGlyph(char c);
    int GetLineHeight() const;
    FT_Vector GetKerning(char c, char prev_char);

    FontMetrics GetMetrics(); // REQUIRED
    void SetSize(int pixelSize);
    int GetCurrentSize() const { return currentSize; }

private:
    int currentSize;
    FT_Library ft;
    FT_Face face;

    std::unordered_map<char, Glyph> cache;

    void LoadGlyph(char c);


};