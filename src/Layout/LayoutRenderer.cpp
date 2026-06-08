//
// Created by tkdtu on 5/31/2026.
//

#include "LayoutRenderer.h"

#include <algorithm>
#include <cassert>
#include <functional>

#include "Context/FontManager.h"


LayoutRenderer::LayoutRenderer(RendererSurface &renderer) : renderer(renderer)
{
pickTarget = IRenderBackend::GetRenderBackend().get()->CreateRenderTarget(renderer.GetWidth(), renderer.GetHeight(), false);
}

void LayoutRenderer::resize(int x, int y)
{
}

void LayoutRenderer::RenderRoot(LayoutBox &root) {
    renderer.Clear(Body->computedStyle.backgroundColor);
    Render(root);

    // --- Pick pass ---
    if (pickTarget) {
        renderer.PushTarget(pickTarget);
        renderer.Clear({0, 0, 0, 0});
        isPickPass = true;
        Render(root);
        isPickPass = false;
        renderer.PopTarget();
    }
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

void LayoutRenderer::Render(LayoutBox& box) {
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

    for (auto& child : box.children)
        Render(child);
}


void LayoutRenderer::RenderBox(LayoutBox& box) {
    switch (box.kind) {
        case BoxKind::Block:   RenderBlock(box);   break;
        case BoxKind::Line:    RenderLine(box, box.fontSize); break;
        case BoxKind::TextRun: IsImageBox(box) ? RenderImage(box) : RenderTextRun(box); break;
    }
    for (auto& child : box.children)
        RenderBox(child);
}

bool LayoutRenderer::IsImageBox(LayoutBox& box) {
    return box.node && (box.node->tag == "img" || box.node->tag == "IMG" || box.node->imageData != nullptr);
}


void LayoutRenderer::RenderImage(LayoutBox& box) const {
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


void LayoutRenderer::RenderTextRun(LayoutBox& box) {
    assert(box.node && box.node->parent);
    if (isPickPass) {
        // Text belongs to parent element for hit-testing purposes
        Color c = AllocPickColor(&box);
        renderer.FillRect(box.x, box.y, box.width, box.height, c);
        return;
    }

    const Style& s = box.node->parent ? box.node->parent->computedStyle : box.node->computedStyle;

    Font* font = nullptr;
    FontMetrics m = FontManager::PrepareFontContext(s, box.fontSize, font, renderer.GetWidth(), renderer.GetHeight());

    int baseline = box.y + m.ascent;
    Color color  = s.color;
    int cursorX  = box.x;
    char prev    = 0;

    for (auto c : box.text.chars) {
        auto kern = font->GetKerning(c.c, prev).x >> 6;
        if (prev) cursorX += kern;
        const Glyph& g = font->GetGlyph(IRenderBackend::GetRenderBackend().get(), c.c);
        renderer.DrawGlyph(cursorX + g.bearingX, baseline - g.bearingY, g, color);

        cursorX += g.advance;
        prev = c.c;
    }
}


void LayoutRenderer::RenderBlock(LayoutBox& box) {
    if (!box.node) return;
    if (isPickPass) {
        Color c = AllocPickColor(&box);
        // Flat rect — no borders, no radius, no style
        renderer.FillRect(box.x, box.y, box.width, box.height, c);
        return;
    }

    const Style& s = box.node->computedStyle;

    int resolved = ResolveFontSizeInherit(box.node, renderer.GetWidth(), renderer.GetHeight());

    if (s.hasBackground)
    {
        // Resolve each corner length relative to the element's width (or height)
        int tl = ResolveLength(s.border_radius_top_left,     static_cast<int>(box.width), renderer.GetWidth(), renderer.GetHeight(), resolved);
        int tr = ResolveLength(s.border_radius_top_right,    static_cast<int>(box.width), renderer.GetWidth(), renderer.GetHeight(), resolved);
        int br = ResolveLength(s.border_radius_bottom_right, static_cast<int>(box.width), renderer.GetWidth(), renderer.GetHeight(), resolved);
        int bl = ResolveLength(s.border_radius_bottom_left,  static_cast<int>(box.width), renderer.GetWidth(), renderer.GetHeight(), resolved);

        renderer.FillRectRounded(
            box.x,
            box.y,
            box.width,
            box.height,
            tl,
            tr,
            br,
            bl,
            s.backgroundColor
        );
    }
    int bLeft   = GetVisibleBorderWidth(s.borderLeft, renderer.GetWidth(), renderer.GetHeight(), resolved);
    int bRight  = GetVisibleBorderWidth(s.borderRight, renderer.GetWidth(), renderer.GetHeight(), resolved);
    int bTop    = GetVisibleBorderWidth(s.borderTop, renderer.GetWidth(), renderer.GetHeight(), resolved);
    int bBottom = GetVisibleBorderWidth(s.borderBottom, renderer.GetWidth(), renderer.GetHeight(), resolved);

    if (bTop    > 0) RenderSingleBorderEdge(s.borderTop,    box.x,                      box.x + box.width,      box.y,                          true, resolved);
    if (bBottom > 0) RenderSingleBorderEdge(s.borderBottom, box.x,                      box.x + box.width,      box.y + box.height - bBottom,   true, resolved);
    if (bLeft   > 0) RenderSingleBorderEdge(s.borderLeft,   box.y + bTop,               box.y + box.height - bBottom, box.x,                    false, resolved);
    if (bRight  > 0) RenderSingleBorderEdge(s.borderRight,  box.y + bTop,               box.y + box.height - bBottom, box.x + box.width - bRight, false, resolved);
}


void LayoutRenderer::RenderSingleBorderEdge(const BorderSide& edge, int start, int end, int fixedCoord, bool isHorizontal, float fontSize) {
    BorderStyle style = edge.borderStyle;
    Color color       = edge.borderColor;
    int thickness     = GetVisibleBorderWidth(edge, renderer.GetWidth(), renderer.GetHeight(), fontSize);

    if (style == BorderStyle::None || style == BorderStyle::Hidden || thickness <= 0)
        return;

    switch (style) {
        case BorderStyle::Solid: {
            if (isHorizontal) renderer.FillRect(start, fixedCoord, end - start, thickness, color);
            else              renderer.FillRect(fixedCoord, start, thickness, end - start, color);
            break;
        }
        case BorderStyle::DoubleBorder: {
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
        case BorderStyle::Dotted: {
            int radius  = std::max(1, thickness / 2);
            int spacing = thickness * 2;
            for (int pos = start + radius; pos <= end - radius; pos += spacing) {
                int cx = isHorizontal ? pos : fixedCoord + radius;
                int cy = isHorizontal ? fixedCoord + radius : pos;
                renderer.DrawCircle(cx, cy, radius, color);
            }
            break;
        }
        case BorderStyle::Dashed: {
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

void LayoutRenderer::RenderLineSelection(
    LayoutBox& line)
{
    if (line.children.empty())
        return;

    struct Rect {
        int x;
        int y;
        int width;
        int height;
    };

    std::vector<Rect> rects;

    bool inSelection = false;

    int selStart = 0;
    int selEnd   = 0;

    int top    = line.y + 1;
    int height = line.height - 2;

    for (size_t i = 0; i < line.children.size(); i++) {

        LayoutBox& run =
            line.children[i];

        if (run.kind != BoxKind::TextRun)
            continue;

        //---------------------------------
        // Determine if run is selected
        //---------------------------------

        bool selected = false;

        for (const auto& ch : run.text.chars) {
            if (ch.highlighted) {
                selected = true;
                break;
            }
        }

        //---------------------------------
        // Start selection
        //---------------------------------

        if (selected) {

            if (!inSelection) {
                selStart = run.x;
                selEnd   = run.x + run.width;
                inSelection = true;
            } else {

                // extend THROUGH spacing
                selEnd = run.x + run.width;
            }

        } else {

            //---------------------------------
            // flush selection
            //---------------------------------

            if (inSelection) {

                rects.push_back({
                    selStart,
                    top,
                    selEnd - selStart,
                    height
                });

                inSelection = false;
            }
        }
    }

    //---------------------------------
    // trailing selection
    //---------------------------------

    if (inSelection) {

        rects.push_back({
            selStart,
            top,
            selEnd - selStart,
            height
        });
    }

    //---------------------------------
    // render
    //---------------------------------

    for (const auto& r : rects) {

        renderer.FillRect(
            r.x,
            r.y,
            r.width,
            r.height,
            Color(215, 120, 0, 140));
    }
}
void LayoutRenderer::RenderLine(LayoutBox& box, int textHeight) {
    if (box.children.empty()) return;
    RenderLineSelection(box);
    bool underline   = false;
    bool lineThrough = false;
    TextDecorationStyle decorStyle = TextDecorationStyle::Solid;
    Color decorColor(0, 0, 0);
    int thickness = 1;

    for (const auto& run : box.children) {
        if (!run.node || !run.node->parent) continue;
        const Style& s = run.node->parent->computedStyle;




        thickness  = ResolveLength(s.textDecorationThickness, textHeight, renderer.GetWidth(), renderer.GetHeight(), ResolveFontSizeInherit(run.node, renderer.GetWidth(), renderer.GetHeight()));
        decorColor = s.textDecorationColor;
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

LayoutBox* LayoutRenderer::HitTest(int x, int y)
{
    if (!pickTarget) return nullptr;
    Color c = renderer.ReadPixel(pickTarget, x, y, true);  // glReadPixels under the hood

    if (c.a == 0) return nullptr;
    auto it = pickMap.find(DecodePickID(c));
    return it != pickMap.end() ? it->second : nullptr;
}

LayoutBox* FindTextRunAtPoint(
    LayoutBox& box,
    int x,
    int y)
{
    if (box.kind == BoxKind::TextRun) {

        bool inside =
            x >= box.x &&
            x < box.x + box.width &&
            y >= box.y &&
            y < box.y + box.height;

        if (inside)
            return &box;
    }

    for (LayoutBox& child : box.children) {

        if (auto* r =
            FindTextRunAtPoint(child, x, y))
        {
            return r;
        }
    }

    return nullptr;
}
int LayoutRenderer::GetCharacterOffsetAtX(
    LayoutBox& run,
    int mouseX)
{
    if (run.text.chars.empty())
        return 0;

    const Style& s =
        run.node->parent
            ? run.node->parent->computedStyle
            : run.node->computedStyle;

    Font* font = nullptr;

    FontMetrics m =
        FontManager::PrepareFontContext(
            s,
            run.fontSize,
            font,
            renderer.GetWidth(),
            renderer.GetHeight());

    int cursorX = run.x;
    char prev = 0;

    for (size_t i = 0; i < run.text.chars.size(); i++) {

        const auto& c =
            run.text.chars[i];

        auto kern =
            font->GetKerning(c.c, prev).x >> 6;

        if (prev)
            cursorX += kern;

        const Glyph& g =
            font->GetGlyph(
                IRenderBackend::GetRenderBackend().get(),
                c.c);

        //---------------------------------
        // glyph bounds
        //---------------------------------

        int glyphLeft =
            cursorX;

        int glyphRight =
            cursorX + g.advance;

        //---------------------------------
        // midpoint hit test
        //---------------------------------

        int midpoint =
            (glyphLeft + glyphRight) / 2;

        if (mouseX < midpoint)
            return static_cast<int>(i);

        cursorX += g.advance;
        prev = c.c;
    }

    //---------------------------------
    // after final char
    //---------------------------------

    return static_cast<int>(
        run.text.chars.size());
}
TextHitResult LayoutRenderer::HitTestTextPosition(
    LayoutBox& root,
    int mouseX,
    int mouseY)
{
    auto* run =
        FindTextRunAtPoint(
            root,
            mouseX,
            mouseY);

    if (!run)
        return {};

    int offset =
        GetCharacterOffsetAtX(
            *run,
            mouseX);

    return {
        run,
        offset,
        true
    };
}

Color LayoutRenderer::AllocPickColor(LayoutBox* node) {
    // Check if already assigned
    for (auto& [id, n] : pickMap)
        if (n == node) return EncodePickID(id);

    uint32_t id = nextPickID++;
    pickMap[id] = node;
    return EncodePickID(id);
}
