//
// Created by tkdtu on 5/27/2026.
//

#include "Font.h"

#include "Render/Backend/IRendererBackend.h"

Font::Font(const std::string& path, int pixelSize) {

    FT_Init_FreeType(&ft);
    FT_New_Face(ft, path.c_str(), 0, &face);
    currentSize = pixelSize;
    FT_Set_Pixel_Sizes(face, 0, pixelSize);
}

Font::~Font() {
}

// Add these to your Font private member definitions in Font.h:
// mutable GLuint atlasID = 0;
// mutable int atlasX = 0;
// mutable int atlasY = 0;
// mutable int maxRowHeight = 0;
// const int ATLAS_SIZE = 512;

void Font::LoadGlyph(IRenderBackend* backend, char c) const {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)) {
        return;
    }

    FT_GlyphSlot g = face->glyph;
    Glyph glyph;

    glyph.width    = g->bitmap.width;
    glyph.height   = g->bitmap.rows;
    glyph.bearingX = g->bitmap_left;
    glyph.bearingY = g->bitmap_top;
    glyph.advance  = g->advance.x >> 6;

    // Software path fallback data
    glyph.bitmap.resize(glyph.width * glyph.height);
    for (int i = 0; i < glyph.width * glyph.height; ++i) {
        glyph.bitmap[i] = g->bitmap.buffer[i];
    }

    // Hardware Path: Pack into the abstract atlas
    if (glyph.width > 0 && glyph.height > 0 && backend != nullptr) {

        // If the atlas doesn't exist yet for this font size, create it!
        if (fontTextureID == 0) {
            fontTextureID = backend->CreateFontAtlas(ATLAS_SIZE, ATLAS_SIZE);
        }

        // Row packing layout arithmetic
        if (atlasX + glyph.width >= ATLAS_SIZE) {
            atlasX = 0;
            atlasY += maxRowHeight + 1;
            maxRowHeight = 0;
        }

        // Upload using our generic backend function
        backend->UpdateTextureSubImage(fontTextureID, atlasX, atlasY,
                                       glyph.width, glyph.height, glyph.bitmap.data());

        // Map abstract UV texture coordinates
        glyph.u0 = static_cast<float>(atlasX) / static_cast<float>(ATLAS_SIZE);
        glyph.v0 = static_cast<float>(atlasY) / static_cast<float>(ATLAS_SIZE);
        glyph.u1 = static_cast<float>(atlasX + glyph.width) / static_cast<float>(ATLAS_SIZE);
        glyph.v1 = static_cast<float>(atlasY + glyph.height) / static_cast<float>(ATLAS_SIZE);

        glyph.textureAssetID = fontTextureID; // Assign it to the glyph!

        atlasX += glyph.width + 1;
        if (glyph.height > maxRowHeight) {
            maxRowHeight = glyph.height;
        }
    } else {
        // Space characters or fallback safely
        glyph.textureAssetID = 0;
        glyph.u0 = glyph.v0 = glyph.u1 = glyph.v1 = 0.0f;
    }

    cache[c] = glyph;
}

int Font::GetLineHeight() const {
    return (face->size->metrics.ascender - face->size->metrics.descender) >> 6;
}

FT_Vector Font::GetKerning(char c, char prev_char) const {
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
FontMetrics Font::GetMetrics() const {
    FontMetrics m;

    m.ascent  = face->size->metrics.ascender >> 6;
    m.descent = -(face->size->metrics.descender >> 6);
    m.lineGap = (face->size->metrics.height >> 6) - (m.ascent + m.descent);

    m.lineHeight = m.ascent + m.descent + m.lineGap;

    return m;
}
const Glyph& Font::GetGlyph(IRenderBackend* backend, char c) const {
    if (cache.find(c) == cache.end()) {
        LoadGlyph(backend, c);
    }
    return cache[c];
}

void Font::SetSize(IRenderBackend* backend, int pixelSize) {
    if (pixelSize == currentSize) return;
    currentSize = pixelSize;

    // Tell the backend to clean up the texture resource if it exists
    // (You can add a virtual backend->DestroyTexture(fontTextureID) to your interface later if needed!)
    fontTextureID = 0;
    atlasX = 0;
    atlasY = 0;
    maxRowHeight = 0;

    cache.clear(); // Clear glyph cache since they are size-specific
    FT_Set_Pixel_Sizes(face, 0, pixelSize);
}