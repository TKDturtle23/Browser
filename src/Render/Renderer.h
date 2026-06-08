#pragma once

#include <vector>
#include <cstdint>

#include "../Color.h"
#include "../Text/Font.h"
#include "Backend/IRendererBackend.h"
#include "Backend/RenderTarget.h"

class RendererSurface {
public:

    RendererSurface(
        int width,
        int height
    );

    ~RendererSurface();

    void Resize(int width, int height);

    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

    void Clear(Color color);

    void FillRect(
        int x,
        int y,
        int w,
        int h,
        Color color
    );

    void DrawLine(
        int x0,
        int y0,
        int x1,
        int y1,
        int thickness,
        Color color
    );

    void DrawLine(
        int x0,
        int y0,
        int x1,
        int y1,
        Color color
    );

    void DrawPixel(
        int x,
        int y,
        Color color
    );

    void DrawGlyph(
        int x,
        int y,
        const Glyph& glyph,
        Color color
    );

    void DrawRect(
        int x,
        int y,
        int w,
        int h,
        Color color
    );

    void FillRectBeveled(
        int x,
        int y,
        int w,
        int h,
        int radius,
        Color color
    );
    void FillRectRounded(
        int x,
        int y,
        int w,
        int h,
        int radius_top_left,
        int radius_top_right,
        int radius_bottom_left,
        int radius_bottom_right,
        Color color);
    void DrawCircle(
        int cx,
        int cy,
        int radius,
        Color color
    );

    void DrawWavyLine(
        int startX,
        int y,
        int endX,
        int amplitude,
        int wavelength,
        Color color
    );

    void DrawWavyLine(
        int startX,
        float startY,
        int endX,
        float endY,
        float amplitude,
        float frequency,
        int thickness,
        Color color
    );

    void BlitFrom(
        const RendererSurface& source,
        int dstX,
        int dstY,
        int srcX,
        int srcY,
        int w,
        int h
    );

    RenderTargetID GetTargetID() const;

private:

    RenderTargetID target;

    int width;
    int height;

    std::shared_ptr<IRenderBackend> backend;
};
