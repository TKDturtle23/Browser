// LayoutRenderer.cpp

#include "LayoutRenderer.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <functional>
#include <iostream>
#include <unordered_set>

// ===========================================================================
//  Small utilities
// ===========================================================================

static bool IsBlank(const std::string& s) {
    return std::all_of(s.begin(), s.end(),
        [](char c) { return std::isspace(static_cast<unsigned char>(c)); });
}

static bool IsNonRendered(const std::string& tag) {
    static const std::unordered_set<std::string> kTags = {
        "head", "title", "meta", "link", "script", "style", "base", "noscript"
    };
    return kTags.contains(tag);
}

static bool IsInlineTag(const std::string& tag) {
    static const std::unordered_set<std::string> kTags = {
        "span", "a", "b", "i", "em", "strong", "small", "code",
        "u", "s", "label", "mark", "sub", "sup", "font", "tt"
    };
    return kTags.contains(tag);
}

// A node participates in inline flow if it is a text node or a known inline
// element.  Unknown tags default to block so we don't accidentally inline them.
static bool IsInlineChild(const Node& n) {
    if (n.type == NodeType::Text)    return true;
    if (n.type != NodeType::Element) return false;
    if (n.computedStyle.display == DisplayType::Block) return false;
    return IsInlineTag(n.tag);
}


// ===========================================================================
//  Word — the atomic unit of inline layout
// ===========================================================================

struct Word {
    const Node* node  = nullptr;   // text node that owns this word
    std::string text;
    int         width = 0;
    int         fontSize = 0;
    bool        hasSpaceBefore = false;
    bool        bold   = false;
    bool        italic = false;
};


// ===========================================================================
//  WordCollector — walks an inline subtree and emits Words
// ===========================================================================

class WordCollector {
public:
    WordCollector(Font& base, Font& italic, Font& bold, Font& boldItalic,
                  std::vector<Word>& out,
                  std::function<Font&(const Style&)> resolveFont)
        : base_(base), italic_(italic), bold_(bold), boldItalic_(boldItalic)
        , out_(out)
        , resolveFont_(std::move(resolveFont))
    {}

    void Visit(const Node& node) {
        if (node.type == NodeType::Element && IsNonRendered(node.tag))
            return;

        if (node.type == NodeType::Text) {
            VisitText(node);
            return;
        }

        if (node.type == NodeType::Element) {
            Font& font = resolveFont_(node.computedStyle);
            font.SetSize(node.computedStyle.font_size > 0
                ? node.computedStyle.font_size : 16);

            for (const auto& child : node.children) {
                // Block children inside an inline context are skipped here;
                // they will be handled by the block formatter.
                if (child->type == NodeType::Element
                    && child->computedStyle.display == DisplayType::Block)
                    continue;
                Visit(*child);
            }
        }
    }

private:
    void VisitText(const Node& node) {
        assert(node.parent);
        const Style& parentStyle = node.parent->computedStyle;
        Font& font = resolveFont_(parentStyle);

        int size = parentStyle.font_size > 0 ? parentStyle.font_size : 16;
        font.SetSize(size);

        const std::string& t = node.text;
        size_t i = 0;
        while (i < t.size()) {
            // Consume leading whitespace; remember we saw it.
            if (std::isspace(static_cast<unsigned char>(t[i]))) {
                pendingSpace_ = true;
                while (i < t.size() && std::isspace(static_cast<unsigned char>(t[i])))
                    ++i;
                continue;
            }

            // Consume a non-whitespace run.
            size_t start = i;
            while (i < t.size() && !std::isspace(static_cast<unsigned char>(t[i])))
                ++i;

            Word w;
            w.node           = &node;
            w.text           = t.substr(start, i - start);
            w.width          = MeasureText(font, w.text);
            w.fontSize       = font.GetCurrentSize();
            w.hasSpaceBefore = pendingSpace_;
            w.bold           = parentStyle.font_bold;
            w.italic         = parentStyle.font_italic;

            out_.push_back(std::move(w));
            pendingSpace_ = false;
        }
    }

    static int MeasureText(Font& font, const std::string& s) {
        int w = 0;
        char prev = 0;
        for (char c : s) {
            if (prev) w += font.GetKerning(c, prev).x >> 6;
            w += font.GetGlyph(c).advance;
            prev = c;
        }
        return w;
    }

    Font& base_;
    Font& italic_;
    Font& bold_;
    Font& boldItalic_;
    std::vector<Word>& out_;
    std::function<Font&(const Style&)> resolveFont_;
    bool pendingSpace_ = false;
};


// ===========================================================================
//  InlineFormattingContext — normal inline / block-in-inline layout
// ===========================================================================

class InlineFormattingContext : public FormattingContext {
public:
    InlineFormattingContext(LayoutRenderer& lr, TextAlign align)
        : lr_(lr), align_(align) {}

    // Lays out `inlineRoots` into lines starting at (startX, startY) within
    // `containerWidth`.  Appends line boxes to `parent`.
    // Returns the Y coordinate just below the last line.
    int LayoutRoots(const std::vector<const Node*>& inlineRoots,
                    LayoutBox& parent,
                    int startX, int startY, int containerWidth)
    {
        // --- 1. Collect words ------------------------------------------------
        std::vector<Word> words;
        WordCollector wc(
            lr_.BaseFont, lr_.BaseItalicFont,
            lr_.BaseBoldFont, lr_.BaseBoldItalicFont,
            words,
            [&](const Style& s) -> Font& { return lr_.ResolveFont(s); }
        );
        for (const Node* n : inlineRoots)
            wc.Visit(*n);

        // --- 2. Line-break words into lines ----------------------------------
        Font& baseFont   = lr_.BaseFont;
        FontMetrics base = baseFont.GetMetrics();
        int spaceWidth   = baseFont.GetGlyph(' ').advance;
        int rightEdge    = startX + containerWidth;

        auto MakeLine = [&](int y) {
            LayoutBox line;
            line.kind  = BoxKind::Line;
            line.x     = startX;
            line.y     = y;
            line.width = containerWidth;
            line.height = base.lineHeight; // refined below
            return line;
        };

        std::vector<LayoutBox> lines;
        LayoutBox currentLine = MakeLine(startY);
        int cursorX = startX;

        for (const Word& w : words) {
            bool lineHasContent = !currentLine.children.empty();
            int  gap = (lineHasContent && w.hasSpaceBefore) ? spaceWidth : 0;

            if (lineHasContent && cursorX + gap + w.width > rightEdge) {
                FinalizeLineMetrics(currentLine);
                lines.push_back(std::move(currentLine));
                currentLine = MakeLine(lines.back().y + lines.back().height);
                cursorX = startX;
                gap     = 0;
            }

            cursorX += gap;

            Font&       font = lr_.ResolveFont(w.node->parent->computedStyle);
            font.SetSize(w.fontSize);
            FontMetrics wm   = font.GetMetrics();

            LayoutBox run;
            run.kind     = BoxKind::TextRun;
            run.x        = cursorX;
            run.y        = currentLine.y;
            run.width    = w.width;
            run.height   = wm.lineHeight;
            run.fontSize = w.fontSize;
            run.node     = w.node;
            run.text     = w.text;

            currentLine.children.push_back(std::move(run));
            cursorX += w.width;
        }

        if (!currentLine.children.empty()) {
            FinalizeLineMetrics(currentLine);
            lines.push_back(std::move(currentLine));
        }

        // --- 3. Apply text-align ---------------------------------------------
        if (align_ != TextAlign::Left) {
            for (auto& line : lines) {
                int contentRight = line.children.back().x + line.children.back().width;
                int slack        = containerWidth - (contentRight - line.x);
                if (slack <= 0) continue;

                int offset = (align_ == TextAlign::Center) ? slack / 2 : slack;
                for (auto& run : line.children)
                    run.x += offset;
            }
        }

        // --- 4. Move lines into parent ----------------------------------------
        int nextY = startY;
        for (auto& line : lines) {
            nextY = line.y + line.height;
            parent.children.push_back(std::move(line));
        }
        return nextY;
    }

    // FormattingContext interface (not used directly for inline — call
    // LayoutRoots instead; this satisfies the vtable).
    int Layout(const Node&, LayoutBox&, int, int contentY, int) override {
        return contentY;
    }

private:
    static void FinalizeLineMetrics(LayoutBox& line) {
        int ascent = 0, descent = 0, maxH = 0;
        for (const auto& run : line.children) {
            maxH   = std::max(maxH, run.height);
            // ascent/descent are stored per-run if we want per-glyph baseline
            // alignment; for now approximate from height
        }
        if (maxH > 0) line.height = maxH;
        // lineAscent / lineDescent populated below if available
    }

    LayoutRenderer& lr_;
    TextAlign       align_;
};


// ===========================================================================
//  BlockFormattingContext — normal block layout (current behaviour)
// ===========================================================================

class BlockFormattingContext : public FormattingContext {
public:
    explicit BlockFormattingContext(LayoutRenderer& lr) : lr_(lr) {}

    int Layout(const Node& node,
               LayoutBox& parent,
               int contentX, int contentY, int contentWidth) override
    {
        int cursorY = contentY;
        const auto& kids = node.children;
        size_t i = 0;

        while (i < kids.size()) {
            const Node& child = *kids[i];

            if (ShouldSkip(child)) { ++i; continue; }

            if (IsInlineChild(child)) {
                i = LayoutInlineRun(kids, i, parent,
                                    contentX, cursorY, contentWidth,
                                    node.computedStyle.textAlign,
                                    cursorY);
            } else {
                LayoutBox cb = lr_.LayoutBlock(child, contentX, cursorY, contentWidth);
                cursorY = cb.y + cb.height + child.computedStyle.margin_bottom;
                parent.children.push_back(std::move(cb));
                ++i;
            }
        }
        return cursorY;
    }

private:
    // Returns true for nodes that produce no layout output.
    static bool ShouldSkip(const Node& n) {
        return n.type == NodeType::Doctype
            || n.type == NodeType::Document
            || (n.type == NodeType::Element && IsNonRendered(n.tag));
    }

    // Collects a contiguous run of inline children starting at `start`,
    // lays them out, and returns the index of the first non-inline child.
    size_t LayoutInlineRun(
        const std::vector<std::unique_ptr<Node>>& kids,
        size_t start,
        LayoutBox& parent,
        int contentX, int contentY, int contentWidth,
        TextAlign align,
        int& cursorY)
    {
        std::vector<const Node*> run;
        size_t j = start;

        while (j < kids.size()) {
            const Node& c = *kids[j];
            if (ShouldSkip(c))      { ++j; continue; }
            if (!IsInlineChild(c))  break;
            if (c.type == NodeType::Text && IsBlank(c.text)) { ++j; continue; }
            run.push_back(&c);
            ++j;
        }

        if (!run.empty()) {
            InlineFormattingContext ifc(lr_, align);
            cursorY = ifc.LayoutRoots(run, parent, contentX, contentY, contentWidth);
        }
        return j;
    }

    LayoutRenderer& lr_;
};


// ===========================================================================
//  LayoutRenderer — public interface
// ===========================================================================

LayoutRenderer::LayoutRenderer(Renderer& renderer)
    : renderer(renderer)
    , BaseFont        ("arial/ARIAL.TTF",   16)
    , BaseItalicFont  ("arial/ARIALI.TTF",  16)
    , BaseBoldFont    ("arial/ARIALBD.TTF", 16)
    , BaseBoldItalicFont("arial/ARIALBI.TTF", 16)
{}

Font& LayoutRenderer::ResolveFont(const Style& s) {
    if (s.font_bold && s.font_italic) return BaseBoldItalicFont;
    if (s.font_bold)                  return BaseBoldFont;
    if (s.font_italic)                return BaseItalicFont;
    return BaseFont;
}

// ---------------------------------------------------------------------------
//  Layout entry point
// ---------------------------------------------------------------------------

LayoutBox LayoutRenderer::LayoutBlock(const Node& node,
                                      int containerX,
                                      int containerY,
                                      int containerWidth)
{
    const Style& s = node.computedStyle;

    LayoutBox box;
    box.kind = BoxKind::Block;
    box.node = &node;

    // --- 1. Compute Horizontal Frame Padding/Borders -----------------------
    int paddingX = s.padding_left + s.padding_right;
    int borderX  = s.border.left + s.border.right;

    // --- 2. Resolve Outer Box Width -----------------------------------------
    if (s.width >= 0) {
        if (s.boxSizing == BoxSizing::ContentBox) {
            // Content-box: specified width is just the inside; expand outward
            box.width = s.width + paddingX + borderX;
        } else {
            // Border-box: specified width is the final outer width
            box.width = s.width;
        }
    } else {
        // width: auto takes up all available space minus margins
        box.width = containerWidth - s.margin_left - s.margin_right;
    }

    // --- 3. Apply Horizontal Constraints ------------------------------------
    if (s.max_width >= 0) box.width = std::min(box.width, s.max_width);
    if (s.min_width >= 0) box.width = std::max(box.width, s.min_width);

    // --- 4. Position the Box Horizontally -----------------------------------
    if (s.margin_left_auto && s.margin_right_auto) {
        box.x = containerX + (containerWidth - box.width) / 2;
    } else {
        box.x = containerX + s.margin_left;
    }
    box.y = containerY + s.margin_top;

    // --- 5. Determine Inner Content Context ---------------------------------
    int contentX     = box.x + s.border.left + s.padding_left;
    int contentY     = box.y + s.border.top  + s.padding_top;
    int contentWidth = std::max(0, box.width - paddingX - borderX);

    // --- 6. Choose Formatting Context & Layout Children ---------------------
    std::unique_ptr<FormattingContext> ctx;
    switch (s.display) {
        default: ctx = std::make_unique<BlockFormattingContext>(*this); break;
    }

    int endY = ctx->Layout(node, box, contentX, contentY, contentWidth);

    // --- 7. Resolve Outer Box Height ----------------------------------------
    int paddingY = s.padding_top + s.padding_bottom;
    int borderY  = s.border.top + s.border.bottom;

    if (s.height >= 0) {
        if (s.boxSizing == BoxSizing::ContentBox) {
            box.height = s.height + paddingY + borderY;
        } else {
            box.height = s.height;
        }
    } else {
        // height: auto is determined by the bottom of the children content
        box.height = (endY - contentY) + paddingY + borderY;
    }

    // --- 8. Apply Vertical Constraints --------------------------------------
    if (s.max_height >= 0) box.height = std::min(box.height, s.max_height);
    if (s.min_height >= 0) box.height = std::max(box.height, s.min_height);

    return box;
}

// ---------------------------------------------------------------------------
//  DOM → layout root
// ---------------------------------------------------------------------------

void LayoutRenderer::Update(const Node& dom) {
    // Find <body> anywhere in the tree.
    const Node* body = nullptr;
    std::function<const Node*(const Node&)> findBody = [&](const Node& n) -> const Node* {
        for (const auto& child : n.children) {
            if (child->tag == "body")        return child.get();
            if (auto* r = findBody(*child))  return r;
        }
        return nullptr;
    };
    body = findBody(dom);
    assert(body && "DOM must contain a <body> element");

    root = LayoutBlock(*body, 0, 0, renderer.GetWidth());

    // Ensure the root fills the viewport so the background covers the window.
    root.height = std::max(root.height, renderer.GetHeight());
}

// ===========================================================================
//  Rendering
// ===========================================================================

Color LayoutRenderer::FindWindowBackground() const {
    std::function<Color(const LayoutBox&)> find = [&](const LayoutBox& b) -> Color {
        if (b.node && b.node->computedStyle.hasBackground)
            return b.node->computedStyle.backgroundColor;
        for (const auto& c : b.children) {
            Color r = find(c);
            if (r.a != 0) return r; // found something
        }
        return Color(0, 0, 0, 0); // sentinel: nothing found
    };
    Color bg = find(root);
    return bg.a != 0 ? bg : Color(255, 255, 255);
}

void LayoutRenderer::Render() {
    renderer.Clear(FindWindowBackground());
    RenderBox(root);
}

void LayoutRenderer::RenderBox(const LayoutBox& box) {
    switch (box.kind) {
        case BoxKind::Block:   RenderBlock(box);   break;
        case BoxKind::Line:    RenderLine(box);     break;
        case BoxKind::TextRun: RenderTextRun(box);  break;
    }
    for (const auto& child : box.children)
        RenderBox(child);
}

// ---------------------------------------------------------------------------

void LayoutRenderer::RenderTextRun(const LayoutBox& box) {
    assert(box.node && box.node->parent);
    const Style& s = box.node->parent->computedStyle;

    Font& font = ResolveFont(s);
    if (box.fontSize > 0) font.SetSize(box.fontSize);

    FontMetrics m  = font.GetMetrics();
    int baseline   = box.y + m.ascent;
    Color color    = s.color;

    int cursorX = box.x;
    char prev   = 0;
    for (char c : box.text) {
        if (prev) cursorX += font.GetKerning(c, prev).x >> 6;
        const Glyph& g = font.GetGlyph(c);
        renderer.DrawGlyph(cursorX + g.bearingX, baseline - g.bearingY, g, color);
        cursorX += g.advance;
        prev = c;
    }
}

// ---------------------------------------------------------------------------

void LayoutRenderer::RenderBlock(const LayoutBox& box) {
    if (!box.node) return;
    const Style& s = box.node->computedStyle;

    if (s.hasBackground)
        renderer.FillRect(box.x, box.y, box.width, box.height, s.backgroundColor);

    if (s.border.any()) {
        const Border& b = s.border;
        if (b.top)    renderer.FillRect(box.x,                       box.y,                        box.width,  b.top,    b.color);
        if (b.bottom) renderer.FillRect(box.x,                       box.y + box.height - b.bottom, box.width, b.bottom, b.color);
        if (b.left)   renderer.FillRect(box.x,                       box.y,                        b.left,    box.height, b.color);
        if (b.right)  renderer.FillRect(box.x + box.width - b.right, box.y,                        b.right,   box.height, b.color);
    }
}

// ---------------------------------------------------------------------------

void LayoutRenderer::RenderLine(const LayoutBox& box) {
    if (box.children.empty()) return;

    // Check if any run in this line needs a text decoration.
    bool underline    = false;
    bool lineThrough  = false;
    TextDecorationStyle decorStyle = TextDecorationStyle::Solid;
    Color decorColor(0, 0, 0);
    int   thickness   = 1;

    for (const auto& run : box.children) {
        if (!run.node || !run.node->parent) continue;
        const Style& s = run.node->parent->computedStyle;
        thickness  = s.TextDecorationThickness;
        decorColor = s.TextDecorationColor;
        decorStyle = s.textDecorationStyle;

        if (s.textDecoration == TextDecoration::Underline)   { underline   = true; break; }
        if (s.textDecoration == TextDecoration::LineThrough) { lineThrough = true; break; }
    }

    if (!underline && !lineThrough) return;

    int startX   = box.children.front().x;
    int endX     = box.children.back().x + box.children.back().width;
    int baseline = box.y + box.lineAscent;
    int y        = underline ? baseline + 2
                             : box.y + (box.lineAscent + box.lineDescent) / 2;

    RenderDecoration(box, startX, endX, y);
}

// ---------------------------------------------------------------------------
// All decoration styles isolated here — add new ones without touching
// RenderLine().
void LayoutRenderer::RenderDecoration(const LayoutBox& box,
                                       int startX, int endX,
                                       int y)
{
    if (box.children.empty() || !box.children.front().node) return;
    const Style& s = box.children.front().node->parent->computedStyle;

    TextDecorationStyle style = s.textDecorationStyle;
    Color color               = s.TextDecorationColor;
    int   thickness           = s.TextDecorationThickness;

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
            for (int ty = 0; ty < thickness; ++ty) {
                for (int x = startX; x < endX; x += dashLen + gapLen)
                    renderer.DrawLine(x, y + ty, std::min(x + dashLen, endX), y + ty, color);
            }
            break;
        }
        case TextDecorationStyle::Wavy: {
            float amplitude = std::max(1.5f, thickness * 1.5f);
            float frequency = static_cast<float>(endX - startX) / 24.0f;
            renderer.DrawWavyLine(startX, y + amplitude + 1,
                                  endX,   y + amplitude + 1,
                                  amplitude, frequency, thickness, color);
            break;
        }
    }
}