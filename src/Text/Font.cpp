//
// Created by tkdtu on 5/27/2026.
//

#include "Font.h"

Font::Font(const std::string& path, int pixelSize) {

    FT_Init_FreeType(&ft);
    FT_New_Face(ft, path.c_str(), 0, &face);
    currentSize = pixelSize;
    FT_Set_Pixel_Sizes(face, 0, pixelSize);
}

Font::~Font() {
}

void Font::LoadGlyph(char c) {

    if (FT_Load_Char(face, c, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)) {
        return;
    }

    FT_GlyphSlot g = face->glyph;

    Glyph glyph;

    glyph.width = g->bitmap.width;
    glyph.height = g->bitmap.rows;

    glyph.bearingX = g->bitmap_left;
    glyph.bearingY = g->bitmap_top;

    glyph.advance = g->advance.x >> 6;

    glyph.bitmap.resize(glyph.width * glyph.height);

    for (int y = 0; y < glyph.height; y++) {
        for (int x = 0; x < glyph.width; x++) {
            glyph.bitmap[y * glyph.width + x] =
                g->bitmap.buffer[y * glyph.width + x];
        }
    }

    cache[c] = glyph;
}
int Font::GetLineHeight() const {
    return (face->size->metrics.ascender - face->size->metrics.descender) >> 6;
}

FT_Vector Font::GetKerning(char c, char prev_char) {
    FT_Vector delta;
    delta.x = 0;
    delta.y = 0;

    if (!FT_HAS_KERNING(face))
        return delta;

    FT_UInt left = FT_Get_Char_Index(face, prev_char);
    FT_UInt right = FT_Get_Char_Index(face, c);

    FT_Get_Kerning(
        face,
        left,
        right,
        FT_KERNING_DEFAULT,
        &delta
    );

    return delta;
}
FontMetrics Font::GetMetrics()
{
    FontMetrics m;

    m.ascent  = face->size->metrics.ascender >> 6;
    m.descent = -(face->size->metrics.descender >> 6);
    m.lineGap = face->size->metrics.height >> 6
              - (m.ascent + m.descent);

    m.lineHeight = m.ascent + m.descent + m.lineGap;

    return m;
}
const Glyph& Font::GetGlyph(char c) {

    if (cache.find(c) == cache.end()) {
        LoadGlyph(c);
    }

    return cache[c];
}

void Font::SetSize(int pixelSize) {
    if (pixelSize == currentSize) return;
    currentSize = pixelSize;
    cache.clear(); // glyphs are size-specific
    FT_Set_Pixel_Sizes(face, 0, pixelSize);
}
