// RendererSurface.cpp

#include "Renderer.h"

#include <algorithm>
#include <cmath>

RendererSurface::RendererSurface(
    int width,
    int height
)
    : width(width),
      height(height)
{
    backend = IRenderBackend::GetRenderBackend();

    target = backend->CreateRenderTarget(
        width,
        height
    );
}

RendererSurface::~RendererSurface() {
    if (backend) {
        backend->DestroyRenderTarget(target);
    }
}

void RendererSurface::Resize(
    int w,
    int h
) {
    width = w;
    height = h;

    backend->ResizeRenderTarget(
        target,
        width,
        height
    );
}

void RendererSurface::Clear(Color color) {
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::Clear;
    cmd.clear = {
        .color = color
    };

    backend->SubmitCommand(cmd);
}

void RendererSurface::FillRect(
    int x,
    int y,
    int w,
    int h,
    Color color
) {
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::FillRect;
    cmd.fillRect = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .color = color
    };

    backend->SubmitCommand(cmd);
}

void RendererSurface::DrawLine(
    int x0,
    int y0,
    int x1,
    int y1,
    int thickness,
    Color color
) {
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::DrawLine;
    cmd.drawLine = {
        .x0 = x0,
        .y0 = y0,
        .x1 = x1,
        .y1 = y1,
        .thickness = thickness,
        .color = color
    };

    backend->SubmitCommand(cmd);
}

void RendererSurface::DrawLine(
    int x0,
    int y0,
    int x1,
    int y1,
    Color color
) {
    DrawLine(x0, y0, x1, y1, 1, color);
}

void RendererSurface::DrawPixel(
    int x,
    int y,
    Color color
) {
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::DrawPixel;
    cmd.drawPixel = {
        .x = x,
        .y = y,
        .color = color
    };

    backend->SubmitCommand(cmd);
}

void RendererSurface::DrawGlyph(
    int x,
    int y,
    const Glyph& glyph,
    Color color
) {
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::DrawGlyph;
    cmd.drawGlyph = {
        .x = x,
        .y = y,
        .glyph = glyph,
        .color = color
    };

    backend->SubmitCommand(cmd);
}

void RendererSurface::DrawRect(
    int x,
    int y,
    int w,
    int h,
    Color color
) {
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::DrawRect;
    cmd.drawRect = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .color = color
    };

    backend->SubmitCommand(cmd);
}

void RendererSurface::FillRectBeveled(
    int x,
    int y,
    int w,
    int h,
    int radius,
    Color color
) {
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::FillRectBeveled;
    cmd.fillRectBeveled = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .radius = radius,
        .color = color
    };

    backend->SubmitCommand(cmd);
}

void RendererSurface::FillRectRounded(int x, int y, int w, int h, int radius_top_left, int radius_top_right,
    int radius_bottom_left, int radius_bottom_right, Color color)
{
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::FillRectRounded;
    cmd.fillRectRounded = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .tl = radius_top_left,
        .tr = radius_top_right,
        .bl = radius_bottom_left,
        .br = radius_bottom_right,
        .color = color
    };

    backend->SubmitCommand(cmd);
}

void RendererSurface::DrawCircle(
    int cx,
    int cy,
    int radius,
    Color color
) {
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::DrawCircle;
    cmd.drawCircle = {
        .cx = cx,
        .cy = cy,
        .radius = radius,
        .color = color
    };

    backend->SubmitCommand(cmd);
}

void RendererSurface::DrawWavyLine(
    int startX,
    int y,
    int endX,
    int amplitude,
    int wavelength,
    Color color
) {
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::DrawWavyLineInt;
    cmd.drawWavyLineInt = {
        .startX = startX,
        .y = y,
        .endX = endX,
        .amplitude = amplitude,
        .wavelength = wavelength,
        .color = color
    };

    backend->SubmitCommand(cmd);
}

void RendererSurface::DrawWavyLine(
    int startX,
    float startY,
    int endX,
    float endY,
    float amplitude,
    float frequency,
    int thickness,
    Color color
) {
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::DrawWavyLineFloat;
    cmd.drawWavyLineFloat = {
        .startX = startX,
        .endX = endX,
        .startY = startY,
        .endY = endY,
        .amplitude = amplitude,
        .frequency = frequency,
        .thickness = thickness,
        .color = color
    };

    backend->SubmitCommand(cmd);
}

void RendererSurface::BlitFrom(
    const RendererSurface& source,
    int dstX,
    int dstY,
    int srcX,
    int srcY,
    int w,
    int h
) {
    RenderCommand cmd{};
    cmd.target = target;
    cmd.type = RenderCommandType::BlitRenderTarget;
    cmd.blitRenderTarget = {
        .source = source.GetTargetID(),
        .dstX = dstX,
        .dstY = dstY,
        .srcX = srcX,
        .srcY = srcY,
        .w = w,
        .h = h
    };

    backend->SubmitCommand(cmd);
}

RenderTargetID RendererSurface::GetTargetID() const {
    return target;
}