
#include "Renderer.h"
#include <algorithm>



Renderer::Renderer(int width, int height)
    : width(width),
      height(height),
      frontBuffer(width * height),
      backBuffer(width * height) {}
void Renderer::Resize(int w, int h) {
    width = w;
    height = h;

    frontBuffer.assign(width * height, Color());
    backBuffer.assign(width * height, Color());
}

void Renderer::Clear(Color color) {
    std::fill(backBuffer.begin(), backBuffer.end(), color);
}

void Renderer::DrawPixel(int x, int y, Color color) {
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;

    backBuffer[y * width + x] = color;
}

void Renderer::FillRect(int x, int y, int w, int h, Color color) {
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            DrawPixel(px, py, color);
        }
    }
}
void Renderer::DrawRect(
    int x,
    int y,
    int w,
    int h,
    Color color
) {
    // top
    for (int px = x; px < x + w; px++) {
        DrawPixel(px, y, color);
    }

    // bottom
    for (int px = x; px < x + w; px++) {
        DrawPixel(px, y + h - 1, color);
    }

    // left
    for (int py = y; py < y + h; py++) {
        DrawPixel(x, py, color);
    }

    // right
    for (int py = y; py < y + h; py++) {
        DrawPixel(x + w - 1, py, color);
    }
}
void Renderer::FillRectWithBorder(
    int x,
    int y,
    int w,
    int h,
    Color fill,
    Color border
) {
    FillRect(x, y, w, h, fill);
    DrawRect(x, y, w, h, border);
}
void Renderer::Present() {
    std::swap(frontBuffer, backBuffer);
}
const std::vector<Color>& Renderer::GetFrontBuffer() const {
    return frontBuffer;
}
int Renderer::GetWidth() const {
    return width;
}

int Renderer::GetHeight() const {
    return height;
}

void Renderer::DrawGlyph(
    int x,
    int y,
    const Glyph& glyph,
    Color color
) {
    
    for (int gy = 0; gy < glyph.height; gy++) {
        for (int gx = 0; gx < glyph.width; gx++) {

            unsigned char alpha = glyph.bitmap[gy * glyph.width + gx];
            if (alpha == 0) continue;

            int px = x + gx;
            int py = y + gy;
            if (px < 0 || py < 0 || px >= width || py >= height) continue;

            if (alpha == 255) {
                backBuffer[py * width + px] = color;
                continue;
            }
            // blend glyph color over existing background pixel
            Color& dst = backBuffer[py * width + px];
            float a = alpha / 255.0f;
            dst.r = static_cast<uint8_t>(color.r * a + dst.r * (1.0f - a));
            dst.g = static_cast<uint8_t>(color.g * a + dst.g * (1.0f - a));
            dst.b = static_cast<uint8_t>(color.b * a + dst.b * (1.0f - a));
        }
    }
}