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

    void FillRectBeveled(int x, int y, int w, int h, int bevelSize, Color color);

    void DrawRectBeveled(int x, int y, int w, int h, int bevelSize, Color color);

    void DrawLine(int x0, int y0, int x1, int y1, Color color);

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
    void DrawCircle(int cx, int cy, int radius, Color color); // draws an anti-aliased circle
    void DrawWavyLine(
        int x0, int y0,
        int x1, int y1,
        float amplitude,
        float frequency,
        int thickness,
        Color color
    );

    void CopyFromBuffer(int x, int y, int w, int h, const std::vector<Color> &buffer);
private:
    int width;
    int height;

    std::vector<Color> frontBuffer;
    std::vector<Color> backBuffer;
};