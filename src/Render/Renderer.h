#pragma once

#include <vector>
#include <cstdint>
#include "../Color.h"
#include "../Text/Font.h"
class Renderer {
public:
    Renderer(int width, int height);

    void Resize(int width, int height);

    void Clear(Color color);

    void DrawPixel(int x, int y, Color color);

    void FillRect(int x, int y, int w, int h, Color color);
    void DrawRect(
        int x,
        int y,
        int w,
        int h,
        Color color
    );
    void FillRectWithBorder(
    int x,
    int y,
    int w,
    int h,
    Color fill,
    Color border
);
    void Present(); // swap buffers

    const std::vector<Color>& GetFrontBuffer() const;
    void DrawGlyph(
        int x,
        int y,
        const Glyph& glyph,
        Color color
    );
    int GetWidth() const;
    int GetHeight() const;

private:
    int width;
    int height;

    std::vector<Color> frontBuffer;
    std::vector<Color> backBuffer;
};