//
// Created by tkdtu on 5/27/2026.
//

#include "Font.h"

#include <cstring>
#include <format>

#include "Render/Backend/IRendererBackend.h"

std::u32string Utf8ToUtf32(std::string_view input) {
    std::u32string result;
    result.reserve(input.size()); // worst case 1:1

    size_t i = 0;

    while (i < input.size())
    {
        uint8_t byte = static_cast<uint8_t>(input[i]);
        char32_t codepoint;
        int extra;

        if (byte <= 0x7F)
        {
            codepoint = byte;
            extra     = 0;
        }
        else if ((byte & 0xE0) == 0xC0)
        {
            codepoint = byte & 0x1F;
            extra     = 1;
        }
        else if ((byte & 0xF0) == 0xE0)
        {
            codepoint = byte & 0x0F;
            extra     = 2;
        }
        else if ((byte & 0xF8) == 0xF0)
        {
            codepoint = byte & 0x07;
            extra     = 3;
        }
        else
        {
            throw std::runtime_error(
                std::format("invalid UTF-8 lead byte 0x{:02X} at index {}", byte, i));
        }

        ++i;

        if (i + extra > input.size())
            throw std::runtime_error(
                std::format("unexpected end of input at index {}", i));

        for (int j = 0; j < extra; ++j, ++i)
        {
            uint8_t cont = static_cast<uint8_t>(input[i]);

            if ((cont & 0xC0) != 0x80)
                throw std::runtime_error(
                    std::format("invalid UTF-8 continuation byte 0x{:02X} at index {}", cont, i));

            codepoint = (codepoint << 6) | (cont & 0x3F);
        }

        // Overlong encodings
        if ((extra == 1 && codepoint < 0x80)   ||
            (extra == 2 && codepoint < 0x800)  ||
            (extra == 3 && codepoint < 0x10000))
            throw std::runtime_error(
                std::format("overlong UTF-8 encoding at index {}", i));

        // Surrogates (U+D800–U+DFFF) are not valid codepoints
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
            throw std::runtime_error(
                std::format("surrogate codepoint U+{:04X} in UTF-8 at index {}", (uint32_t)codepoint, i));

        if (codepoint > 0x10FFFF)
            throw std::runtime_error(
                std::format("codepoint U+{:X} exceeds Unicode range at index {}", (uint32_t)codepoint, i));

        result.push_back(codepoint);
    }

    return result;
}

Font::Font(const std::string& path, int pixelSize) {

    FT_Init_FreeType(&ft);
    FT_New_Face(ft, path.c_str(), 0, &face);
    currentSize = pixelSize;
    FT_Set_Pixel_Sizes(face, 0, pixelSize);
}

Font::~Font() {
}

void Font::LoadGlyph(IRenderBackend* backend, char32_t c) const {
    if (FT_Load_Char(face, c, FT_LOAD_COLOR | FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)) {
        return;
    }

    FT_GlyphSlot g = face->glyph;
    Glyph glyph;

    glyph.width    = g->bitmap.width;
    glyph.height   = g->bitmap.rows;
    glyph.bearingX = g->bitmap_left;
    glyph.bearingY = g->bitmap_top;
    glyph.advance  = g->advance.x >> 6;

    glyph.bitmap.resize(glyph.width * glyph.height * 4);

    if (g->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
    {
        for (int y = 0; y < glyph.height; ++y)
        {
            for (int x = 0; x < glyph.width; ++x)
            {
                uint8_t a =
                    g->bitmap.buffer[y * g->bitmap.pitch + x];

                size_t dst = (y * glyph.width + x) * 4;

                glyph.bitmap[dst + 0] = 255; // B
                glyph.bitmap[dst + 1] = 255; // G
                glyph.bitmap[dst + 2] = 255; // R
                glyph.bitmap[dst + 3] = a;   // A
            }
        }
    }
    else if (g->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA)
    {
        for (int y = 0; y < glyph.height; ++y)
        {
            std::memcpy(
                &glyph.bitmap[y * glyph.width * 4],
                g->bitmap.buffer + y * g->bitmap.pitch,
                glyph.width * 4
            );
        }
    }
    else
    {
        return;
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

FT_Vector Font::GetKerning(char32_t c, char32_t prev_char) const {
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
const Glyph& Font::GetGlyph(IRenderBackend* backend, char32_t c) const {
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

bool Font::HasSymbol(char32_t c) const {
    return (FT_Get_Char_Index(face, c) != 0);
}
