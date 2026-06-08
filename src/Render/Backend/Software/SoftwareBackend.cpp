// SoftwareBackend.cpp

#include "SoftwareBackend.h"

#include <algorithm>
#include <cmath>

#include "Platform/Platform.h"
#include "Platform/Platform_Win32.h"

WindowID SoftwareBackend::RegisterWindow(Platform* context
) {
    const WindowID id = nextWindowID++;

    windows[id] = {
        .platform = context,
        .target = InvalidRenderTarget
    };

    return id;
}

RenderTargetID SoftwareBackend::CreateRenderTarget(
    int width,
    int height
) {
    const RenderTargetID id = nextTargetID++;

    SoftwareRenderTarget target{};

    target.width = width;
    target.height = height;

    target.frontBuffer.resize(width * height);
    target.backBuffer.resize(width * height);

    renderTargets[id] = std::move(target);

    return id;
}

void SoftwareBackend::AttachRenderTarget(
    WindowID window,
    RenderTargetID target
) {
    auto it = windows.find(window);

    if (it == windows.end()) {
        return;
    }

    it->second.target = target;
}

void SoftwareBackend::UnregisterWindow(
    WindowID window
) {
    windows.erase(window);
}

void SoftwareBackend::DestroyRenderTarget(
    RenderTargetID target
) {
    renderTargets.erase(target);
}

void SoftwareBackend::ResizeRenderTarget(
    RenderTargetID target,
    int width,
    int height
) {
    auto it = renderTargets.find(target);

    if (it == renderTargets.end()) {
        return;
    }

    auto& rt = it->second;

    rt.width = width;
    rt.height = height;

    rt.frontBuffer.assign(width * height, {});
    rt.backBuffer.assign(width * height, {});
}

void SoftwareBackend::BeginFrame() {
    commandBuffer.clear();
}

void SoftwareBackend::SubmitCommand(
    const RenderCommand& cmd
) {
    commandBuffer.push_back(cmd);
}

void SoftwareBackend::EndFrame() {
    for (const auto& cmd : commandBuffer) {
        ExecuteCommand(cmd);
    }

    commandBuffer.clear();

    for (auto& [id, target] : renderTargets) {
        std::swap(
            target.frontBuffer,
            target.backBuffer
        );
    }
}

#ifdef _WIN32
#include <Windows.h>
#endif

void SoftwareBackend::Present() {
    for (auto& [windowID, window] : windows) {
        if (!window.platform) {
            continue;
        }

        if (window.target == InvalidRenderTarget) {
            continue;
        }

        auto targetIt = renderTargets.find(window.target);

        if (targetIt == renderTargets.end()) {
            continue;
        }

        auto& target = targetIt->second;

#ifdef _WIN32
        static_cast<Platform_Win32*>(windows[windowID].platform)->Present(
            std::vector<Color>(target.frontBuffer.data(), target.frontBuffer.data() + target.width * target.height)
        );
#endif
    }
}

static inline void DrawPixel(
    SoftwareRenderTarget& target,
    int x,
    int y,
    Color color
) {
    if (x < 0 || y < 0 ||
        x >= target.width ||
        y >= target.height)
    {
        return;
    }

    Color& dst = target.backBuffer[y * target.width + x];

    if (color.a == 255) {
        dst = color;
        return;
    }

    if (color.a == 0) {
        return;
    }

    dst.r = ((color.r * color.a) + (dst.r * (255 - color.a))) / 255;
    dst.g = ((color.g * color.a) + (dst.g * (255 - color.a))) / 255;
    dst.b = ((color.b * color.a) + (dst.b * (255 - color.a))) / 255;
    dst.a = 255;
}
// Internal rounded rectangle filler helper routine used back-end side
static inline void FillRectRoundedInternal(
    SoftwareRenderTarget& target,
    int rx, int ry, int rw, int rh,
    int tl, int tr, int bl, int br, Color color
) {
    // Clamp radii to prevent overlapping allocations or logic corruption
    tl = std::max(0, std::min(tl, std::min(rw, rh) / 2));
    tr = std::max(0, std::min(tr, std::min(rw - tl, rh) / 2));
    bl = std::max(0, std::min(bl, std::min(rw, rh - tl) / 2));
    br = std::max(0, std::min(br, std::min(rw - bl, rh - tr) / 2));

    const int startY = std::max(0, ry);
    const int endY   = std::min(target.height, ry + rh);

    if (rw <= 0 || rh <= 0 || startY >= endY || color.a == 0) {
        return;
    }

    for (int py = startY; py < endY; py++) {
        // Relative vertical offset from the top and bottom bounds of the rect
        int localY = py - ry;

        // Calculate dynamic edge insets based on circular quadrant positions
        int leftInset = 0;
        int rightInset = 0;

        // 1. Top Section Corners
        if (localY < tl && tl > 0) {
            int dy = tl - localY;
            leftInset = tl - static_cast<int>(std::sqrt(tl * tl - dy * dy));
        }
        if (localY < tr && tr > 0) {
            int dy = tr - localY;
            rightInset = tr - static_cast<int>(std::sqrt(tr * tr - dy * dy));
        }

        // 2. Bottom Section Corners
        if (localY >= (rh - bl) && bl > 0) {
            int dy = localY - (rh - bl);
            leftInset = bl - static_cast<int>(std::sqrt(bl * bl - dy * dy));
        }
        if (localY >= (rh - br) && br > 0) {
            int dy = localY - (rh - br);
            rightInset = br - static_cast<int>(std::sqrt(br * br - dy * dy));
        }

        // Clip scanning columns per line to target safety dimensions
        int startX = std::max(0, rx + leftInset);
        int endX   = std::min(target.width, rx + rw - rightInset);

        if (startX >= endX) {
            continue;
        }

        Color* row = &target.backBuffer[py * target.width];
        for (int px = startX; px < endX; px++) {
            if (color.a == 255) {
                row[px] = color;
            } else {
                Color& dst = row[px];
                dst.r = ((color.r * color.a) + (dst.r * (255 - color.a))) / 255;
                dst.g = ((color.g * color.a) + (dst.g * (255 - color.a))) / 255;
                dst.b = ((color.b * color.a) + (dst.b * (255 - color.a))) / 255;
                dst.a = 255;
            }
        }
    }
}
// Internal line segment drawing helper routine used back-end side
// to keep high-level primitives compact and efficient.
static inline void DrawLineInternal(
    SoftwareRenderTarget& target,
    int x0, int y0, int x1, int y1,
    int thickness, Color color
) {
    // Basic fallback implementation for standard and multi-thickness segments
    int dx = std::abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        if (thickness <= 1) {
            DrawPixel(target, x0, y0, color);
        } else {
            int halfThick = thickness / 2;
            for (int ty = -halfThick; ty <= halfThick; ++ty) {
                for (int tx = -halfThick; tx <= halfThick; ++tx) {
                    DrawPixel(target, x0 + tx, y0 + ty, color);
                }
            }
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }

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

// Internal block rectangle filler helper routine used back-end side
static inline void FillRectInternal(
    SoftwareRenderTarget& target,
    int rx, int ry, int rw, int rh, Color color
) {
    const int startX = std::max(0, rx);
    const int startY = std::max(0, ry);
    const int endX   = std::min(target.width, rx + rw);
    const int endY   = std::min(target.height, ry + rh);

    if (startX >= endX || startY >= endY || color.a == 0) {
        return;
    }

    for (int py = startY; py < endY; py++) {
        Color* row = &target.backBuffer[py * target.width];
        for (int px = startX; px < endX; px++) {
            if (color.a == 255) {
                row[px] = color;
            } else {
                Color& dst = row[px];
                dst.r = ((color.r * color.a) + (dst.r * (255 - color.a))) / 255;
                dst.g = ((color.g * color.a) + (dst.g * (255 - color.a))) / 255;
                dst.b = ((color.b * color.a) + (dst.b * (255 - color.a))) / 255;
                dst.a = 255;
            }
        }
    }
}

static inline Color WithAlpha(
    Color color,
    uint8_t alpha
) {
    color.a = static_cast<uint8_t>((static_cast<int>(color.a) * alpha) / 255);
    return color;
}

void SoftwareBackend::ExecuteCommand(
    const RenderCommand& cmd
) {
    auto targetIt = renderTargets.find(cmd.target);

    if (targetIt == renderTargets.end()) {
        return;
    }

    auto& target = targetIt->second;

    switch (cmd.type) {
    case RenderCommandType::Clear:
    {
        std::fill(
            target.backBuffer.begin(),
            target.backBuffer.end(),
            cmd.clear.color
        );
        break;
    }

    case RenderCommandType::FillRect:
    {
        const auto& r = cmd.fillRect;
        FillRectInternal(target, r.x, r.y, r.w, r.h, r.color);
        break;
    }
    case RenderCommandType::FillRectRounded:
        {
            const auto& r = cmd.fillRectRounded;
            FillRectRoundedInternal(target, r.x, r.y, r.w, r.h, r.tl, r.tr, r.bl, r.br, r.color);
            break;
        }
    case RenderCommandType::DrawLine:
    {
        const auto& l = cmd.drawLine;
        DrawLineInternal(target, l.x0, l.y0, l.x1, l.y1, l.thickness, l.color);
        break;
    }

    case RenderCommandType::DrawPixel:
    {
        const auto& p = cmd.drawPixel;
        DrawPixel(target, p.x, p.y, p.color);
        break;
    }

    case RenderCommandType::DrawGlyph:
    {
        const auto& g = cmd.drawGlyph;
        const Glyph& glyph = g.glyph;

        for (int gy = 0; gy < glyph.height; ++gy) {
            for (int gx = 0; gx < glyph.width; ++gx) {
                const int index = gy * glyph.width + gx;

                if (index < 0 || index >= static_cast<int>(glyph.bitmap.size())) {
                    continue;
                }

                const uint8_t alpha = glyph.bitmap[index];
                if (alpha == 0) {
                    continue;
                }

                DrawPixel(
                    target,
                    g.x + gx,
                    g.y + gy,
                    WithAlpha(g.color, alpha)
                );
            }
        }
        break;
    }

    case RenderCommandType::BlitRenderTarget:
    {
        const auto& b = cmd.blitRenderTarget;
        auto sourceIt = renderTargets.find(b.source);

        if (sourceIt == renderTargets.end()) {
            break;
        }

        auto& source = sourceIt->second;

        for (int y = 0; y < b.h; ++y) {
            const int srcY = b.srcY + y;
            const int dstY = b.dstY + y;

            if (srcY < 0 || srcY >= source.height || dstY < 0 || dstY >= target.height) {
                continue;
            }

            for (int x = 0; x < b.w; ++x) {
                const int srcX = b.srcX + x;
                const int dstX = b.dstX + x;

                if (srcX < 0 || srcX >= source.width || dstX < 0 || dstX >= target.width) {
                    continue;
                }

                DrawPixel(
                    target,
                    dstX,
                    dstY,
                    source.backBuffer[srcY * source.width + srcX]
                );
            }
        }
        break;
    }

    case RenderCommandType::DrawRect:
    {
        const auto& r = cmd.drawRect;
        DrawLineInternal(target, r.x, r.y, r.x + r.w - 1, r.y, 1, r.color);
        DrawLineInternal(target, r.x, r.y + r.h - 1, r.x + r.w - 1, r.y + r.h - 1, 1, r.color);
        DrawLineInternal(target, r.x, r.y, r.x, r.y + r.h - 1, 1, r.color);
        DrawLineInternal(target, r.x + r.w - 1, r.y, r.x + r.w - 1, r.y + r.h - 1, 1, r.color);
        break;
    }

    case RenderCommandType::FillRectBeveled:
    {
        const auto& r = cmd.fillRectBeveled;
        int radius = std::max(0, std::min(r.radius, std::min(r.w, r.h) / 2));

        if (radius == 0) {
            FillRectInternal(target, r.x, r.y, r.w, r.h, r.color);
            break;
        }

        FillRectInternal(target, r.x + radius, r.y, r.w - radius * 2, r.h, r.color);
        FillRectInternal(target, r.x, r.y + radius, radius, r.h - radius * 2, r.color);
        FillRectInternal(target, r.x + r.w - radius, r.y + radius, radius, r.h - radius * 2, r.color);

        for (int dy = 0; dy < radius; ++dy) {
            const int inset = radius - dy - 1;
            FillRectInternal(target, r.x + inset, r.y + dy, radius - inset, 1, r.color);
            FillRectInternal(target, r.x + r.w - radius, r.y + dy, radius - inset, 1, r.color);
            FillRectInternal(target, r.x + inset, r.y + r.h - dy - 1, radius - inset, 1, r.color);
            FillRectInternal(target, r.x + r.w - radius, r.y + r.h - dy - 1, radius - inset, 1, r.color);
        }
        break;
    }

    case RenderCommandType::DrawCircle:
    {
        const auto& c = cmd.drawCircle;
        int x = c.radius;
        int y = 0;
        int err = 0;

        while (x >= y) {
            DrawPixel(target, c.cx + x, c.cy + y, c.color);
            DrawPixel(target, c.cx + y, c.cy + x, c.color);
            DrawPixel(target, c.cx - y, c.cy + x, c.color);
            DrawPixel(target, c.cx - x, c.cy + y, c.color);
            DrawPixel(target, c.cx - x, c.cy - y, c.color);
            DrawPixel(target, c.cx - y, c.cy - x, c.color);
            DrawPixel(target, c.cx + y, c.cy - x, c.color);
            DrawPixel(target, c.cx + x, c.cy - y, c.color);

            if (err <= 0) {
                ++y;
                err += 2 * y + 1;
            }
            if (err > 0) {
                --x;
                err -= 2 * x + 1;
            }
        }
        break;
    }

    case RenderCommandType::DrawWavyLineInt:
    {
        const auto& w = cmd.drawWavyLineInt;
        if (w.wavelength <= 0) {
            DrawLineInternal(target, w.startX, w.y, w.endX, w.y, 1, w.color);
            break;
        }

        int prevX = w.startX;
        int prevY = w.y;

        for (int x = w.startX + 1; x <= w.endX; ++x) {
            const double phase = static_cast<double>(x - w.startX) / static_cast<double>(w.wavelength);
            const int nextY = w.y + static_cast<int>(std::round(std::sin(phase * 2.0 * 3.14159265358979323846) * w.amplitude));

            DrawLineInternal(target, prevX, prevY, x, nextY, 1, w.color);
            prevX = x;
            prevY = nextY;
        }
        break;
    }

    case RenderCommandType::DrawWavyLineFloat:
    {
        const auto& w = cmd.drawWavyLineFloat;
        if (w.endX <= w.startX) {
            break;
        }

        if (w.frequency == 0.0f) {
            DrawLineInternal(target, w.startX, static_cast<int>(std::round(w.startY)),
                             w.endX, static_cast<int>(std::round(w.endY)),
                             w.thickness, w.color);
            break;
        }

        int prevX = w.startX;
        int prevY = static_cast<int>(std::round(w.startY));

        for (int x = w.startX + 1; x <= w.endX; ++x) {
            const float t = static_cast<float>(x - w.startX) / static_cast<float>(w.endX - w.startX);
            const float baseY = w.startY + (w.endY - w.startY) * t;
            const float waveY = baseY + std::sin(static_cast<float>(x - w.startX) * w.frequency) * w.amplitude;

            const int nextY = static_cast<int>(std::round(waveY));
            DrawLineInternal(target, prevX, prevY, x, nextY, w.thickness, w.color);
            prevX = x;
            prevY = nextY;
        }
        break;
    }

    default:
        break;
    }
}
// Inside SoftwareBackend.cpp
TextureID SoftwareBackend::CreateFontAtlas(int, int) { return 0; }
void SoftwareBackend::UpdateTextureSubImage(TextureID, int, int, int, int, const uint8_t*) {}