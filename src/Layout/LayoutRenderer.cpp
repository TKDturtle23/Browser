//
// Created by tkdtu on 5/27/2026.
//

#include "LayoutRenderer.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <unordered_set>

LayoutRenderer::LayoutRenderer(Renderer& renderer)
    : renderer(renderer), BaseFont("arial/ARIAL.TTF", 16), BaseItalicFont("arial/ARIALI.TTF", 16), BaseBoldFont("arial/ARIALBD.TTF", 16),
    BaseBoldItalicFont("arial/ARIALBI.TTF", 16){
}

static bool IsBlank(const std::string& s) {
    return std::all_of(s.begin(), s.end(), [](char c) {
        return std::isspace(static_cast<unsigned char>(c));
    });
}

static bool IsNonRendered(const std::string& tag) {
    static const std::unordered_set<std::string> tags = {
        "head", "title", "meta", "link", "script", "style", "base", "noscript"
    };
    return tags.contains(tag);
}

static bool IsInlineTag(const std::string& tag) {
    static const std::unordered_set<std::string> tags = {
        "span", "a", "b", "i", "em", "strong", "small", "code", "u", "s",
        "label", "mark", "sub", "sup", "font", "tt"
    };
    return tags.contains(tag);
}

// Classifies a child node for block-flow layout.
// Treats text nodes and known inline tags as inline; everything else is block.
// (Default Style::display is Inline, so we can't trust it for unknown tags yet.)
static bool IsInlineChild(const Node& n) {
    if (n.type == NodeType::Text) return true;
    if (n.type != NodeType::Element) return false;
    if (n.computedStyle.display == DisplayType::Block) return false;
    return IsInlineTag(n.tag);
}

namespace {

    struct Word {
        const Node* node;
        std::string text;
        int width;
        bool hasSpaceBefore;
        int fontSize;
        bool bold;
        bool italic;
    };

struct WordCollector {
    Font& BaseFont;
    Font& BaseItalicFont;
    Font& BaseBoldFont;
    Font& BaseBoldItalicFont;
    std::vector<Word>& out;
    bool pendingSpace = false;
    Font & ResolveFont(const Style &s) { // copied from LayoutRenderer
        if (s.font_bold && s.font_italic)
            return BaseBoldItalicFont;
        if (s.font_bold)
            return BaseBoldFont;
        if (s.font_italic)
            return BaseItalicFont;
        return BaseFont;
    }
    static int MeasureRun(Font& font, const std::string& s) {
        int w = 0;
        char prev = 0;
        for (char c : s) {
            if (prev != 0) {
                FT_Vector k = font.GetKerning(c, prev);
                w += k.x >> 6;
            }
            w += font.GetGlyph(c).advance;
            prev = c;
        }
        return w;
    }

    void Visit(const Node& node) {
        const Style* s = node.parent ? &node.parent->computedStyle : nullptr;
        Font& font = ResolveFont(*s);

        if (node.type == NodeType::Element && IsNonRendered(node.tag)) return;

        if (node.type == NodeType::Text) {


            int size = (s && s->font_size > 0) ? s->font_size : 16;
            font.SetSize(size);

            const std::string& t = node.text;
            size_t i = 0;
            while (i < t.size()) {
                bool sawWs = false;
                while (i < t.size() && std::isspace(static_cast<unsigned char>(t[i]))) {
                    i++;
                    sawWs = true;
                }
                if (sawWs) pendingSpace = true;

                size_t start = i;
                while (i < t.size() && !std::isspace(static_cast<unsigned char>(t[i]))) i++;
                if (i > start) {
                    Word w;
                    w.node = &node;
                    w.text = t.substr(start, i - start);
                    w.width = MeasureRun(font, w.text);
                    w.hasSpaceBefore = pendingSpace;
                    w.fontSize = font.GetCurrentSize();

                    if (node.parent) {
                        w.bold = node.parent->computedStyle.font_bold;
                        w.italic = node.parent->computedStyle.font_italic;
                    }
                    out.push_back(std::move(w));
                    pendingSpace = false;
                }
            }
            return;
        }

        if (node.type == NodeType::Element) {
            font.SetSize(node.computedStyle.font_size > 0
                ? node.computedStyle.font_size : 16);

            for (const auto& child : node.children) {
                if (child->type == NodeType::Element
                    && child->computedStyle.display == DisplayType::Block)
                    continue;
                Visit(*child);
            }
        }
    }
};

}

std::vector<LayoutBox> LayoutRenderer::LayoutInline(
    const std::vector<const Node*>& inlineRoots,
    int startX,
    int startY,
    int containerWidth,
    TextAlign textAlign,
    int* outNextY)
{
    auto finalizeLineMetrics = [&](LayoutBox& line) {
        int ascent = 0;
        int descent = 0;

        for (const auto& run : line.children) {
            Font& font = ResolveFont(run.node->parent->computedStyle);
            font.SetSize(run.fontSize);
            FontMetrics m = font.GetMetrics();

            ascent = std::max(ascent, m.ascent);
            descent = std::max(descent, m.descent);
        }

        line.lineAscent = ascent;
        line.lineDescent = descent;
    };
    std::vector<Word> words;
    {
        WordCollector wc{BaseFont, BaseItalicFont, BaseBoldFont, BaseBoldItalicFont, words};
        for (const Node* n : inlineRoots) {
            wc.Visit(*n);
        }
    }

    std::vector<LayoutBox> lines;

    FontMetrics m = BaseFont.GetMetrics();
    int lineHeight = m.lineHeight;
    int spaceWidth = BaseFont.GetGlyph(' ').advance;
    int rightEdge = startX + containerWidth;
    int lineAscent = 0;
    int lineDescent = 0;
    auto newLine = [&](int y) {
        LayoutBox line;
        line.kind = BoxKind::Line;
        line.x = startX;
        line.y = y;
        line.width = containerWidth;
        line.height = lineHeight; // will be updated by finalizeLineHeight

        return line;
    };

    LayoutBox currentLine = newLine(startY);
    int cursorX = startX;
    // replace the fixed lineHeight with per-line max
    auto finalizeLineHeight = [&](LayoutBox& line) {
        int maxH = 0;
        for (const auto& run : line.children)
            maxH = std::max(maxH, run.height);
        if (maxH > 0) line.height = maxH;
    };
    for (const Word& w : words) {
        bool lineHasContent = !currentLine.children.empty();
        int gap = (lineHasContent && w.hasSpaceBefore) ? spaceWidth : 0;

        if (lineHasContent && cursorX + gap + w.width > rightEdge) {
            finalizeLineHeight(currentLine);
            finalizeLineMetrics(currentLine);
            lines.push_back(std::move(currentLine));
            currentLine = newLine(lines.back().y + lines.back().height);
            cursorX = startX;
            gap = 0;
        }

        cursorX += gap;

        LayoutBox run;
        run.kind = BoxKind::TextRun;
        run.x = cursorX;
        run.y = currentLine.y;
        run.width = w.width;
        run.fontSize = w.fontSize;  // store it
        Font& font = ResolveFont(w.node->parent->computedStyle);
        font.SetSize(w.fontSize);
        FontMetrics wm = font.GetMetrics();

        run.height = wm.lineHeight;
        run.node = w.node;
        run.text = w.text;
        currentLine.children.push_back(std::move(run));

        cursorX += w.width;
    }

    if (!currentLine.children.empty()) {
        finalizeLineHeight(currentLine);
        finalizeLineMetrics(currentLine);
        lines.push_back(std::move(currentLine));
    }

    // text-align offset pass
    if (textAlign != TextAlign::Left) {
        for (auto& line : lines) {
            // sum up the widths of all runs plus gaps between them
            int contentWidth = 0;
            for (const auto& run : line.children)
                contentWidth = (run.x + run.width) - line.x;
            // contentWidth is now rightmost edge minus line start

            int slack = containerWidth - contentWidth;
            if (slack <= 0) continue;

            int offset = (textAlign == TextAlign::Center) ? slack / 2 : slack;

            for (auto& run : line.children)
                run.x += offset;
        }
    }

    *outNextY = lines.empty()
    ? startY
    : (lines.back().y + lines.back().height);
    return lines;
}

LayoutBox LayoutRenderer::LayoutBlock(const Node& node, int containerX, int containerY, int containerWidth) {
    const Style& s = node.computedStyle;

    LayoutBox box;
    box.kind = BoxKind::Block;
    box.node = &node;
    box.x = containerX + s.margin_left;
    box.y = containerY + s.margin_top;

    int available = containerWidth - s.margin_left - s.margin_right;
    box.width = s.width >= 0 ? s.width : available;

    // horizontal centering: margin: auto on both sides
    if (s.margin_left_auto && s.margin_right_auto) {
        int remaining = containerWidth - box.width;
        box.x = containerX + remaining / 2;
    } else {
        box.x = containerX + s.margin_left;
    }
    // box.width already includes border+padding — no change needed here

    int contentX = box.x + s.border.left + s.padding_left;
    int contentY = box.y + s.border.top  + s.padding_top;
    int contentWidth = box.width
        - s.border.left - s.border.right
        - s.padding_left - s.padding_right;

    int cursorY = contentY;

    const auto& kids = node.children;
    size_t i = 0;
    while (i < kids.size()) {
        const Node& child = *kids[i];

        if (child.type == NodeType::Doctype || child.type == NodeType::Document) {
            i++;
            continue;
        }
        if (child.type == NodeType::Element && IsNonRendered(child.tag)) {
            i++;
            continue;
        }

        if (IsInlineChild(child)) {
            // Skip text nodes that are entirely whitespace — they only matter as
            // separators between adjacent inline siblings, which CollectWords handles.
            std::vector<const Node*> run;
            size_t j = i;
            while (j < kids.size()) {
                const Node& c = *kids[j];
                if (c.type == NodeType::Doctype || c.type == NodeType::Document) break;
                if (c.type == NodeType::Element && IsNonRendered(c.tag)) break;
                if (!IsInlineChild(c)) break;
                if (c.type == NodeType::Text && IsBlank(c.text)) {
                    j++;
                    continue;
                }
                run.push_back(&c);
                j++;
            }

            if (!run.empty()) {
                int nextY = cursorY;
                TextAlign align = node.computedStyle.textAlign;
                auto lines = LayoutInline(run, contentX, cursorY, contentWidth, align, &nextY);
                for (auto& line : lines) {
                    box.children.push_back(std::move(line));
                }
                cursorY = nextY;
            }

            i = j;
        }
        else {
            LayoutBox cb = LayoutBlock(child, contentX, cursorY, contentWidth);
            cursorY = cb.y + cb.height + child.computedStyle.margin_bottom;
            box.children.push_back(std::move(cb));
            i++;
        }
    }

    box.height = s.height >= 0
        ? s.height
        : (cursorY - contentY + s.padding_bottom + s.border.bottom);

    if (s.min_height >= 0)
        box.height = std::max(box.height, s.min_height);
    return box;
}
#include <functional>
void LayoutRenderer::Update(const Node& dom) {
    // find body
    const Node* body = nullptr;

    std::function<const Node*(const Node&)> findBody =
        [&](const Node& n) -> const Node* {
            for (const auto& child : n.children) {
                if (child->tag == "body") {
                    return child.get();
                }

                if (const Node* result = findBody(*child)) {
                    return result;
                }
            }

            return nullptr;
    };

    body = findBody(dom);

    root = LayoutBlock(*body, 0, 0, renderer.GetWidth());
    root.height = std::max(root.height, renderer.GetHeight());

    for (auto& child : root.children) {
        child.height = std::max(child.height, renderer.GetHeight());
        for (auto& grandchild : child.children) {
            if (grandchild.node && grandchild.node->tag == "body")
                grandchild.height = std::max(grandchild.height, renderer.GetHeight());
        }
    }
}
void LayoutRenderer::RenderBox(const LayoutBox& box) {
    if (box.kind == BoxKind::TextRun) {
        Font& font = ResolveFont(box.node->parent->computedStyle);
        if (box.fontSize > 0)
            font.SetSize(box.fontSize);
        Color textColor(0, 0, 0);
        if (box.node && box.node->parent) {
            textColor = box.node->parent->computedStyle.color;
        }
        FontMetrics m = font.GetMetrics();
        int baseline = box.y + m.ascent;

        int cursorX = box.x;
        char prev = 0;
        int StartX = cursorX;
        for (char c : box.text) {
            const Glyph& g = font.GetGlyph(c);

            if (prev != 0) {
                FT_Vector k = font.GetKerning(c, prev);
                cursorX += k.x >> 6;
            }

            renderer.DrawGlyph(
                cursorX + g.bearingX,
                baseline - g.bearingY,
                g,
                textColor
            );

            cursorX += g.advance;
            prev = c;
        }

        return;
    }

    if (box.kind == BoxKind::Block && box.node) {
        const Style& s = box.node->computedStyle;

        // Background fills the border-box (inside the margin)
        if (s.hasBackground) {
            renderer.FillRect(box.x, box.y, box.width, box.height, s.backgroundColor);
        }

        // Border drawn on top of background, inside the box edges
        if (s.border.any()) {
            const Border& b = s.border;
            // Top
            if (b.top > 0)
                renderer.FillRect(box.x, box.y, box.width, b.top, b.color);
            // Bottom
            if (b.bottom > 0)
                renderer.FillRect(box.x, box.y + box.height - b.bottom, box.width, b.bottom, b.color);
            // Left
            if (b.left > 0)
                renderer.FillRect(box.x, box.y, b.left, box.height, b.color);
            // Right
            if (b.right > 0)
                renderer.FillRect(box.x + box.width - b.right, box.y, b.right, box.height, b.color);
        }
    }
    if (box.kind == BoxKind::Line) {
        bool underline = false;
        bool lineThrough = false;
        int thickness = 1;
        TextDecorationStyle style = TextDecorationStyle::Solid;
        Color text_decoration_color(0, 0, 0);
        // find any text decoration in the line
        for (const auto& run : box.children) {
            thickness = run.node->parent->computedStyle.TextDecorationThickness;
            text_decoration_color = run.node->parent->computedStyle.TextDecorationColor;
            style = run.node->parent->computedStyle.textDecorationStyle;
            if (run.node && run.node->parent &&
                run.node->parent->computedStyle.textDecoration == TextDecoration::Underline) {
                underline = true;
                break;
                }
            if (run.node && run.node->parent &&
                run.node->parent->computedStyle.textDecoration == TextDecoration::LineThrough) {
                lineThrough = true;
                break;
                }
        }

        int startX = box.children.front().x;
        const LayoutBox& last = box.children.back();
        int endX = last.x + last.width;
        if ((underline || lineThrough) && !box.children.empty()) {
        Font& font = ResolveFont(box.children.front().node->parent->computedStyle);
        FontMetrics m = font.GetMetrics();
        int baseline = box.y + box.lineAscent; // <-- use stored ascent, not font lookup
        int y;
        if (underline) {
            y = baseline + 2;
        } else if (lineThrough) {
            y = box.y + (box.lineAscent + box.lineDescent) / 2;
        }
        if (!box.children.empty()) {
            switch (style) {
                case TextDecorationStyle::Solid: {
                    for (int i = 0; i < thickness; i++) {
                        renderer.DrawLine(startX, y, endX, y, text_decoration_color);
                        y += 1;
                    } break;
                }
                case TextDecorationStyle::Double: {
                    for (int i = 0; i < thickness; i++) {
                        renderer.DrawLine(startX, y, endX, y, text_decoration_color);
                        y += 1;
                    }
                    y += thickness / 2;
                    for (int i = 0; i < thickness; i++) {
                        renderer.DrawLine(startX, y, endX, y, text_decoration_color);
                        y += 1;
                    }
                    break;
                }
                case TextDecorationStyle::Dotted: {
                    int radius = std::max(1, thickness / 2);

                    // distance between dot centers
                    int spacing = thickness * 2;

                    for (int cx = startX; cx <= endX; cx += spacing) {

                        // draw filled circle
                        for (int oy = -radius; oy <= radius; oy++) {
                            for (int ox = -radius; ox <= radius; ox++) {

                                if (ox * ox + oy * oy <= radius * radius) {
                                    renderer.DrawPixel(
                                        cx + ox,
                                        y + oy,
                                        text_decoration_color
                                    );
                                }
                            }
                        }
                    }

                    break;
                }

                case TextDecorationStyle::Dashed: {
                    int dashLength = std::max(4, thickness * 4);
                    int gapLength  = std::max(2, thickness * 2);

                    for (int ty = 0; ty < thickness; ty++) {

                        int x = startX;

                        while (x < endX) {
                            int dashEnd = std::min(x + dashLength, endX);

                            renderer.DrawLine(
                                x,
                                y + ty,
                                dashEnd,
                                y + ty,
                                text_decoration_color
                            );

                            x += dashLength + gapLength;
                        }
                    }

                    break;
                }

                case TextDecorationStyle::Wavy: {
                    float amplitude = std::max(1.5f, thickness * 1.5f);

                    // number of wave cycles across the line
                    float frequency =
                        static_cast<float>(endX - startX) / 24.0f;

                    renderer.DrawWavyLine(
                        startX,
                        y + amplitude + 1,
                        endX,
                        y + amplitude + 1,
                        amplitude,
                        frequency,
                        thickness,
                        text_decoration_color
                    );

                    break;
                }
            }
        }
        }

    }
    for (const auto& child : box.children) {
        RenderBox(child);
    }
}

Font & LayoutRenderer::ResolveFont(const Style &s) {
    if (s.font_bold && s.font_italic)
        return BaseBoldItalicFont;
    if (s.font_bold)
        return BaseBoldFont;
    if (s.font_italic)
        return BaseItalicFont;
    return BaseFont;
}

// in Render(), replace Clear with:
void LayoutRenderer::Render() {
    // find body/html background for window fill
    Color windowBg(255, 255, 255);
    std::function<bool(const LayoutBox&)> findBg = [&](const LayoutBox& b) -> bool {
        if (b.node && b.node->computedStyle.hasBackground) {
            windowBg = b.node->computedStyle.backgroundColor;
            return true;
        }
        for (const auto& c : b.children)
            if (findBg(c)) return true;
        return false;
    };
    findBg(root);
    renderer.Clear(windowBg);
    RenderBox(root);
}