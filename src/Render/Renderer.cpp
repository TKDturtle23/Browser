
#include "Renderer.h"
#include <algorithm>
#include <cmath>



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
void Renderer::DrawLine(int x0, int y0, int x1, int y1, Color color) {
    int dx = std::abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;

    int dy = -std::abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx + dy; // error term

    while (true) {
        DrawPixel(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Renderer::DrawWavyLine(
    int x0, int y0,
    int x1, int y1,
    float amplitude,
    float frequency,
    int thickness,
    Color color
) {
    // Direction vector
    float dx = static_cast<float>(x1 - x0);
    float dy = static_cast<float>(y1 - y0);

    float length = std::sqrt(dx * dx + dy * dy);
    if (length == 0) return;

    // Normalize direction
    float ux = dx / length;
    float uy = dy / length;

    // Perpendicular vector (for wave offset + thickness)
    float px = -uy;
    float py = ux;

    for (int i = 0; i <= static_cast<int>(length); i++) {
        float t = i / length;

        // Base line point
        float bx = x0 + ux * i;
        float by = y0 + uy * i;

        // Sine wave offset
        float offset = std::sin(t * frequency * 2.0f * 3.14159265f) * amplitude;

        float wx = bx + px * offset;
        float wy = by + py * offset;

        // Draw thickness (disk)
        int r = thickness / 2;

        for (int oy = -r; oy <= r; oy++) {
            for (int ox = -r; ox <= r; ox++) {
                if (ox * ox + oy * oy <= r * r) {
                    DrawPixel(
                        static_cast<int>(wx) + ox,
                        static_cast<int>(wy) + oy,
                        color
                    );
                }
            }
        }
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