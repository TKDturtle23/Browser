//
// Created by tkdtu on 5/31/2026.
//

#include "LayoutRenderer.h"

#include <algorithm>
#include <cassert>
#include <functional>

#include "Context/FontManager.h"


LayoutRenderer::LayoutRenderer(Renderer &renderer) : renderer(renderer){}

void LayoutRenderer::RenderRoot(const LayoutBox &root) {
    renderer.Clear(Body->computedStyle.backgroundColor);
    Render(root);
}

void LayoutRenderer::UpdateDom(Node *dom) {
    Dom = dom;

    // Use a reference to the lambda to avoid overhead, or just standard auto
    std::function<Node*(Node*)> findBody = [&](Node* n) -> Node* {
        if (!n) return nullptr;

        for (const auto& child : n->children) {
            if (child->tag == "body") return child.get();

            if (!IsLayoutIgnored(*child)) {
                if (auto* r = findBody(child.get())) return r;
            }
        }
        return nullptr;
    };

    Body = findBody(Dom);
}

void LayoutRenderer::Render(const LayoutBox& box) {
    if (box.node) {
        box.node->renderData.box.x      = box.x;
        box.node->renderData.box.y      = box.y;
        box.node->renderData.box.width  = box.width;
        box.node->renderData.box.height = box.height;
    }

    switch (box.kind) {
        case BoxKind::Block:
            RenderBlock(box);
            break;
        case BoxKind::Line: {
            int fontSize = 16;
            for (const auto& run : box.children) {
                if (run.fontSize > 0) { fontSize = run.fontSize; break; }
            }
            RenderLine(box, fontSize);
            break;
        }
        case BoxKind::TextRun:
            IsImageBox(box) ? RenderImage(box) : RenderTextRun(box);
            break;
    }

    for (const auto& child : box.children)
        Render(child);
}


void LayoutRenderer::RenderBox(const LayoutBox& box) {
    switch (box.kind) {
        case BoxKind::Block:   RenderBlock(box);   break;
        case BoxKind::Line:    RenderLine(box, box.fontSize); break;
        case BoxKind::TextRun: IsImageBox(box) ? RenderImage(box) : RenderTextRun(box); break;
    }
    for (const auto& child : box.children)
        RenderBox(child);
}

bool LayoutRenderer::IsImageBox(const LayoutBox& box) {
    return box.node && (box.node->tag == "img" || box.node->tag == "IMG" || box.node->imageData != nullptr);
}


void LayoutRenderer::RenderImage(const LayoutBox& box) const {
    if (!box.node) return;

    if (box.node->imageData && box.node->imageData->isLoaded && !box.node->imageData->pixels.empty()) {
        const auto& img = *(box.node->imageData);

        for (int dy = 0; dy < box.height; ++dy) {
            for (int dx = 0; dx < box.width; ++dx) {
                int sx = std::clamp((dx * img.intrinsicWidth)  / box.width,  0, img.intrinsicWidth  - 1);
                int sy = std::clamp((dy * img.intrinsicHeight) / box.height, 0, img.intrinsicHeight - 1);

                Color pixel = img.pixels[sy * img.intrinsicWidth + sx];
                if (pixel.a == 0) continue;
                renderer.DrawPixel(box.x + dx, box.y + dy, pixel);
            }
        }
    } else {
        // Placeholder while image loads
        renderer.FillRect(box.x, box.y, box.width, box.height, Color(245, 245, 245));
        renderer.FillRect(box.x,               box.y,                box.width, 1,         Color(200, 200, 200));
        renderer.FillRect(box.x,               box.y + box.height-1, box.width, 1,         Color(200, 200, 200));
        renderer.FillRect(box.x,               box.y,                1,         box.height, Color(200, 200, 200));
        renderer.FillRect(box.x + box.width-1, box.y,                1,         box.height, Color(200, 200, 200));
    }
}


void LayoutRenderer::RenderTextRun(const LayoutBox& box) {
    assert(box.node && box.node->parent);
    const Style& s = box.node->parent ? box.node->parent->computedStyle : box.node->computedStyle;

    Font* font = nullptr;
    FontMetrics m = FontManager::PrepareFontContext(s, box.fontSize, font, renderer.GetWidth(), renderer.GetHeight());

    int baseline = box.y + m.ascent;
    Color color  = s.color;
    int cursorX  = box.x;
    char prev    = 0;

    for (char c : box.text) {
        if (prev) cursorX += font->GetKerning(c, prev).x >> 6;
        const Glyph& g = font->GetGlyph(c);
        renderer.DrawGlyph(cursorX + g.bearingX, baseline - g.bearingY, g, color);
        cursorX += g.advance;
        prev = c;
    }
}


void LayoutRenderer::RenderBlock(const LayoutBox& box) {
    if (!box.node) return;
    const Style& s = box.node->computedStyle;

    if (s.hasBackground)
        renderer.FillRect(box.x, box.y, box.width, box.height, s.backgroundColor);

    int bLeft   = GetVisibleBorderWidth(s.BorderLeft, renderer.GetWidth(), renderer.GetHeight());
    int bRight  = GetVisibleBorderWidth(s.BorderRight, renderer.GetWidth(), renderer.GetHeight());
    int bTop    = GetVisibleBorderWidth(s.BorderTop, renderer.GetWidth(), renderer.GetHeight());
    int bBottom = GetVisibleBorderWidth(s.BorderBottom, renderer.GetWidth(), renderer.GetHeight());

    if (bTop    > 0) RenderSingleBorderEdge(s.BorderTop,    box.x,                      box.x + box.width,      box.y,                          true);
    if (bBottom > 0) RenderSingleBorderEdge(s.BorderBottom, box.x,                      box.x + box.width,      box.y + box.height - bBottom,   true);
    if (bLeft   > 0) RenderSingleBorderEdge(s.BorderLeft,   box.y + bTop,               box.y + box.height - bBottom, box.x,                    false);
    if (bRight  > 0) RenderSingleBorderEdge(s.BorderRight,  box.y + bTop,               box.y + box.height - bBottom, box.x + box.width - bRight, false);
}


void LayoutRenderer::RenderSingleBorderEdge(const Border_side& edge, int start, int end, int fixedCoord, bool isHorizontal) {
    BorderStyle style = edge.borderStyle;
    Color color       = edge.borderColor;
    int thickness     = GetVisibleBorderWidth(edge, renderer.GetWidth(), renderer.GetHeight());

    if (style == BorderStyle::none || style == BorderStyle::hidden || thickness <= 0)
        return;

    switch (style) {
        case BorderStyle::solid: {
            if (isHorizontal) renderer.FillRect(start, fixedCoord, end - start, thickness, color);
            else              renderer.FillRect(fixedCoord, start, thickness, end - start, color);
            break;
        }
        case BorderStyle::double_border: {
            int line = std::max(1, thickness / 3);
            int gap  = std::max(1, thickness - line * 2);
            if (isHorizontal) {
                renderer.FillRect(start, fixedCoord,             end - start, line, color);
                renderer.FillRect(start, fixedCoord + line + gap, end - start, line, color);
            } else {
                renderer.FillRect(fixedCoord,            start, line, end - start, color);
                renderer.FillRect(fixedCoord + line + gap, start, line, end - start, color);
            }
            break;
        }
        case BorderStyle::dotted: {
            int radius  = std::max(1, thickness / 2);
            int spacing = thickness * 2;
            for (int pos = start + radius; pos <= end - radius; pos += spacing) {
                int cx = isHorizontal ? pos : fixedCoord + radius;
                int cy = isHorizontal ? fixedCoord + radius : pos;
                renderer.DrawCircle(cx, cy, radius, color);
            }
            break;
        }
        case BorderStyle::dashed: {
            int dashLen = std::max(4, thickness * 3);
            int gapLen  = std::max(2, thickness * 2);
            for (int pos = start; pos < end; pos += dashLen + gapLen) {
                int len = std::min(dashLen, end - pos);
                if (isHorizontal) renderer.FillRect(pos, fixedCoord, len, thickness, color);
                else              renderer.FillRect(fixedCoord, pos, thickness, len, color);
            }
            break;
        }
        default: break;
    }
}


void LayoutRenderer::RenderLine(const LayoutBox& box, int textHeight) {
    if (box.children.empty()) return;

    bool underline   = false;
    bool lineThrough = false;
    TextDecorationStyle decorStyle = TextDecorationStyle::Solid;
    Color decorColor(0, 0, 0);
    int thickness = 1;

    for (const auto& run : box.children) {
        if (!run.node || !run.node->parent) continue;
        const Style& s = run.node->parent->computedStyle;
        thickness  = ResolveLength(s.TextDecorationThickness, textHeight, renderer.GetWidth(), renderer.GetHeight());
        decorColor = s.TextDecorationColor;
        decorStyle = s.textDecorationStyle;

        if      (s.textDecoration == TextDecoration::Underline)   { underline   = true; break; }
        else if (s.textDecoration == TextDecoration::LineThrough)  { lineThrough = true; break; }
    }

    if (!underline && !lineThrough) return;

    int startX   = box.children.front().x;
    int endX     = box.children.back().x + box.children.back().width;
    int baseline = box.y + box.lineAscent;
    int y        = underline ? baseline + 2
                             : box.y + (box.lineAscent + box.lineDescent) / 2;

    RenderDecoration(decorStyle, decorColor, thickness, startX, endX, y);
}


void LayoutRenderer::RenderDecoration(TextDecorationStyle style, Color color, int thickness,
                                      int startX, int endX, int y)
{
    switch (style) {
        case TextDecorationStyle::Solid: {
            for (int i = 0; i < thickness; ++i)
                renderer.DrawLine(startX, y + i, endX, y + i, color);
            break;
        }
        case TextDecorationStyle::Double: {
            for (int i = 0; i < thickness; ++i)
                renderer.DrawLine(startX, y + i, endX, y + i, color);
            int gap = std::max(1, thickness / 2);
            int y2  = y + thickness + gap;
            for (int i = 0; i < thickness; ++i)
                renderer.DrawLine(startX, y2 + i, endX, y2 + i, color);
            break;
        }
        case TextDecorationStyle::Dotted: {
            int radius  = std::max(1, thickness / 2);
            int spacing = thickness * 2;
            for (int cx = startX; cx <= endX; cx += spacing)
                for (int oy = -radius; oy <= radius; ++oy)
                    for (int ox = -radius; ox <= radius; ++ox)
                        if (ox*ox + oy*oy <= radius*radius)
                            renderer.DrawPixel(cx + ox, y + oy, color);
            break;
        }
        case TextDecorationStyle::Dashed: {
            int dashLen = std::max(4, thickness * 4);
            int gapLen  = std::max(2, thickness * 2);
            for (int ty = 0; ty < thickness; ++ty)
                for (int x = startX; x < endX; x += dashLen + gapLen)
                    renderer.DrawLine(x, y + ty, std::min(x + dashLen, endX), y + ty, color);
            break;
        }
        case TextDecorationStyle::Wavy: {
            float amplitude = std::max(1.5f, thickness * 1.5f);
            float frequency = static_cast<float>(endX - startX) / 24.0f;
            renderer.DrawWavyLine(startX, y + amplitude + 1, endX, y + amplitude + 1,
                                  amplitude, frequency, thickness, color);
            break;
        }
    }
}