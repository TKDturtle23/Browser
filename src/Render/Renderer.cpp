
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <ostream>


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
    // 1. Bounds checking
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;

    // 2. Performance shortcuts
    if (color.a <= 0) return;       // Fully transparent, nothing to draw
    if (color.a >= 255) {           // Fully opaque, overwrite completely
        backBuffer[y * width + x] = color;
        return;
    }

    // 3. Get the background pixel we are drawing on top of
    Color destColor = backBuffer[y * width + x];

    // 4. Create a new color struct to hold the blended result
    Color blendedColor;

    // 5. Blend each channel using integer math (LERP)
    blendedColor.r = ((color.r * color.a) + (destColor.r * (255 - color.a))) / 255;
    blendedColor.g = ((color.g * color.a) + (destColor.g * (255 - color.a))) / 255;
    blendedColor.b = ((color.b * color.a) + (destColor.b * (255 - color.a))) / 255;

    // 6. Write the final blended pixel to the buffer
    backBuffer[y * width + x] = blendedColor;
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
void Renderer::FillRectBeveled(int x, int y, int w, int h, int bevelSize, Color color) {
    // Clamp bevel size so it doesn't exceed half the dimensions
    if (bevelSize > w / 2) bevelSize = w / 2;
    if (bevelSize > h / 2) bevelSize = h / 2;
    if (bevelSize < 0) bevelSize = 0;

    for (int py = y; py < y + h; py++) {
        // Calculate relative Y coordinates from top and bottom edges
        int dy_top = py - y;
        int dy_bottom = (y + h - 1) - py;

        for (int px = x; px < x + w; px++) {
            // Calculate relative X coordinates from left and right edges
            int dx_left = px - x;
            int dx_right = (x + w - 1) - px;

            // Check top-left corner
            if (dx_left < bevelSize && dy_top < bevelSize && (dx_left + dy_top) < bevelSize) {
                continue;
            }
            // Check top-right corner
            if (dx_right < bevelSize && dy_top < bevelSize && (dx_right + dy_top) < bevelSize) {
                continue;
            }
            // Check bottom-left corner
            if (dx_left < bevelSize && dy_bottom < bevelSize && (dx_left + dy_bottom) < bevelSize) {
                continue;
            }
            // Check bottom-right corner
            if (dx_right < bevelSize && dy_bottom < bevelSize && (dx_right + dy_bottom) < bevelSize) {
                continue;
            }

            DrawPixel(px, py, color);
        }
    }
}

void Renderer::DrawRectBeveled(int x, int y, int w, int h, int bevelSize, Color color) {
    // Clamp bevel size so it doesn't exceed half the dimensions
    if (bevelSize > w / 2) bevelSize = w / 2;
    if (bevelSize > h / 2) bevelSize = h / 2;
    if (bevelSize < 0) bevelSize = 0;

    // --- 1. Straight Edges ---

    // Top edge (indented from left and right)
    for (int px = x + bevelSize; px < x + w - bevelSize; px++) {
        DrawPixel(px, y, color);
    }

    // Bottom edge (indented from left and right)
    for (int px = x + bevelSize; px < x + w - bevelSize; px++) {
        DrawPixel(px, y + h - 1, color);
    }

    // Left edge (indented from top and bottom)
    for (int py = y + bevelSize; py < y + h - bevelSize; py++) {
        DrawPixel(x, py, color);
    }

    // Right edge (indented from top and bottom)
    for (int py = y + bevelSize; py < y + h - bevelSize; py++) {
        DrawPixel(x + w - 1, py, color);
    }

    // --- 2. Beveled Diagonals ---

    for (int i = 0; i < bevelSize; i++) {
        // Top-Left corner
        DrawPixel(x + i, y + bevelSize - 1 - i, color);

        // Top-Right corner
        DrawPixel(x + w - bevelSize + i, y + i, color);

        // Bottom-Left corner
        DrawPixel(x + i, y + h - bevelSize + i, color);

        // Bottom-Right corner
        DrawPixel(x + w - 1 - i, y + h - bevelSize + i, color);
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
        float offset = std::cos((t * frequency * 2.0f * 3.14159265f)  + 3.14159265) * amplitude;

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

void Renderer::CopyFromBuffer(int x, int y, int w, int h, const std::vector<Color> &buffer) {
    if (buffer.size() != w * h) {
        std::cerr << "Buffer size mismatch" << std::endl;
        return;
    }
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            DrawPixel(x + px, y + py, buffer[py * w + px]);
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

void Renderer::DrawCircle(int cx, int cy, int radius, Color color) {
    // Look slightly outside the radius (+1) to capture the anti-aliasing falloff zone
    for (int oy = -radius - 1; oy <= radius + 1; ++oy) {
        for (int ox = -radius - 1; ox <= radius + 1; ++ox) {
            float distance = std::sqrt(static_cast<float>(ox * ox + oy * oy));

            // Inside the solid core
            if (distance <= radius - 0.5f) {
                DrawPixel(cx + ox, cy + oy, color);
            }
            // In the anti-aliasing edge zone (1-pixel transition band)
            else if (distance < radius + 0.5f) {
                float edgeAlpha = (radius + 0.5f) - distance; // Smooth dropoff from 1.0 to 0.0

                Color blendedColor = color;
                // Scale the color's native alpha channel by our edge falloff factor
                blendedColor.a = static_cast<uint8_t>(color.a * edgeAlpha);

                DrawPixel(cx + ox, cy + oy, blendedColor);
            }
        }
    }
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
