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
static int ResolveLength(const CSSLength& len, int referenceContextSize) {
    switch (len.unit) {
        case LengthUnit::Percent:
            return static_cast<int>((len.value / 100.0f) * referenceContextSize);
        case LengthUnit::Px:
            return static_cast<int>(len.value);
        case LengthUnit::Auto:
        case LengthUnit::Em:
        default:
            return 0; // Handled structurally or defaulted safely
    }
}
FontMetrics LayoutRenderer::PrepareFontContext(const Style& s, int forcedSize, Font*& outFont) {
    outFont = &ResolveFont(s);

    int targetSize = 16; // The browser standard default fallback

    if (forcedSize > 0) {
        targetSize = forcedSize;
    } else if (s.font_size.unit != LengthUnit::Auto) {
        // Resolve length relative to a standard 16px parent text context baseline.
        // If it's an 'em' or '%', it scales beautifully (e.g., 1.5em or 150% = 24px)
        if (s.font_size.unit == LengthUnit::Em) {
            targetSize = static_cast<int>(s.font_size.value * 16.0f);
        } else {
            targetSize = ResolveLength(s.font_size, 16);
        }
    }

    // Fallback sanity guard to make sure font sizes never collapse completely or invert
    if (targetSize <= 0) targetSize = 16;

    outFont->SetSize(targetSize);
    return outFont->GetMetrics();
}

static bool IsNonRendered(const std::string& tag) {
    static const std::unordered_set<std::string> kTags = {
        "head", "title", "meta", "link", "script", "style", "base", "noscript"
    };
    return kTags.contains(tag);
}

static bool IsLayoutIgnored(const Node& n) {
    if (n.type == NodeType::Doctype || n.type == NodeType::Document) return true;
    if (n.type == NodeType::Element && IsNonRendered(n.tag)) return true;
    return false;
}



int GetVisibleBorderWidth(const Border_side& side) {
    // If the unit is Auto or less than zero, treat it as invisible/0px width
    if (side.borderWidth.unit == LengthUnit::Auto || side.borderWidth.value < 0.0f) {
        return 0;
    }

    // Resolve the internal value assuming standard 16px parent context conversion
    return ResolveLength(side.borderWidth, 16);
}

bool IsInlineTag(const std::string& tag) {
    std::string lowerTag = tag;
    std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(), ::tolower);

    return (lowerTag == "span" ||
            lowerTag == "a" ||
            lowerTag == "em" ||
            lowerTag == "strong" ||
            lowerTag == "code" ||
            lowerTag == "i" ||
            lowerTag == "b" ||
            lowerTag == "img" ||  // FIX: Allow images to sit inline natively
            lowerTag == "#text");
}
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
    const Node* node  = nullptr;
    std::string text;
    int         width = 0;
    int         height = 0;   // NEW: Track height for atomic replaced boxes
    int         fontSize = 0;
    bool        hasSpaceBefore = false;
    bool        bold   = false;
    bool        italic = false;
    bool        isImage = false; // NEW: Distinguish images from raw strings
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
        if (IsLayoutIgnored(node))
            return;

        if (node.type == NodeType::Text) {
            VisitText(node);
            return;
        }
        if (node.type == NodeType::Element) {
            if (node.tag == "img") {
                Word w;
                w.node = &node;
                w.isImage = true;
                w.hasSpaceBefore = pendingSpace_;

                // Establish dimensions fallback sequence (CSS -> Attributes -> Decoder Intrinsic -> Default)
                int imgW = 32;
                int imgH = 32;

                if (node.attributes.contains("width"))  imgW = std::stoi(node.attributes.at("width"));
                if (node.attributes.contains("height")) imgH = std::stoi(node.attributes.at("height"));

                if (node.computedStyle.width.unit == LengthUnit::Px)  imgW = static_cast<int>(node.computedStyle.width.value);
                if (node.computedStyle.height.unit == LengthUnit::Px) imgH = static_cast<int>(node.computedStyle.height.value);

                // If fully loaded by your custom decoder, grab physical metrics if unstated
                if (node.imageData && node.imageData->isLoaded) {
                    if (!node.attributes.contains("width")  && node.computedStyle.width.unit == LengthUnit::Auto)  imgW = node.imageData->intrinsicWidth;
                    if (!node.attributes.contains("height") && node.computedStyle.height.unit == LengthUnit::Auto) imgH = node.imageData->intrinsicHeight;
                }

                w.width = imgW;
                w.height = imgH;

                out_.push_back(std::move(w));
                pendingSpace_ = false;
                return;
            }


            Font& font = resolveFont_(node.computedStyle);

            // Look up the font size using the exact same rule:
            int targetSize = 16;
            if (node.computedStyle.font_size.unit == LengthUnit::Em) {
                targetSize = static_cast<int>(node.computedStyle.font_size.value * 16.0f);
            } else if (node.computedStyle.font_size.unit != LengthUnit::Auto) {
                targetSize = ResolveLength(node.computedStyle.font_size, 16);
            }
            if (targetSize <= 0) targetSize = 16;

            font.SetSize(targetSize);

            for (const auto& child : node.children) {
                if (child->type == NodeType::Element
                    && child->computedStyle.display == DisplayType::Block)
                    continue;
                Visit(*child);
            }
        }
    }

private:
    void VisitText(const Node& node) {
        const Node* operationalParent = node.parent;
        const Style& parentStyle = operationalParent ? operationalParent->computedStyle : node.computedStyle;
        Font& font = resolveFont_(parentStyle);

        int size = 16;
        if (parentStyle.font_size.unit == LengthUnit::Em) {
            size = static_cast<int>(parentStyle.font_size.value * 16.0f);
        } else if (parentStyle.font_size.unit != LengthUnit::Auto && parentStyle.font_size.value > 0.0f) {
            size = ResolveLength(parentStyle.font_size, 16);
        }
        font.SetSize(size);

        const std::string& t = node.text;

        size_t i = 0;
        while (i < t.size()) {
            if (std::isspace(static_cast<unsigned char>(t[i]))) {
                pendingSpace_ = true;
                while (i < t.size() && std::isspace(static_cast<unsigned char>(t[i])))
                    ++i;
                continue;
            }

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
        if (s.empty()) return 0;

        int w = 0;
        char prev = 0;

        for (size_t i = 0; i < s.size() - 1; ++i) {
            char c = s[i];
            if (prev) w += font.GetKerning(c, prev).x >> 6;
            w += font.GetGlyph(c).advance;
            prev = c;
        }

        char lastChar = s.back();
        if (prev) w += font.GetKerning(lastChar, prev).x >> 6;

        const auto& g = font.GetGlyph(lastChar);
        int lastGlyphVisualWidth = g.bearingX + g.width;

        if (lastGlyphVisualWidth > g.advance) {
            w += lastGlyphVisualWidth;
        } else {
            w += g.advance;
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

    int LayoutRoots(const std::vector<const Node*>& inlineRoots,
                    LayoutBox& parent,
                    int startX, int startY, int containerWidth)
    {
        std::vector<Word> words;
        WordCollector wc(
            lr_.BaseFont, lr_.BaseItalicFont,
            lr_.BaseBoldFont, lr_.BaseBoldItalicFont,
            words,
            [&](const Style& s) -> Font& { return lr_.ResolveFont(s); }
        );
        for (const Node* n : inlineRoots)
            wc.Visit(*n);

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
            line.height = base.lineHeight;
            return line;
        };
std::vector<LayoutBox> lines;
    LayoutBox currentLine = MakeLine(startY);
    int cursorX = startX;

    for (size_t wordIdx = 0; wordIdx < words.size(); ++wordIdx) {
        const Word& w = words[wordIdx];
        const Style& s = w.node->parent ? w.node->parent->computedStyle : w.node->computedStyle;

        bool isNoWrap   = (s.whiteSpace == WhiteSpace::nowrap);
        bool doEllipsis = (s.textOverflow == TextOverflow::Ellipsis);

        // Inside the words vector parsing loop:
        bool lineHasContent = !currentLine.children.empty();
        int gap = (lineHasContent && w.hasSpaceBefore) ? spaceWidth : 0;

        // 1. Line Break wrapping constraints check
        if (!isNoWrap && lineHasContent && cursorX + gap + w.width > rightEdge) {
            FinalizeLineMetrics(currentLine, lr_);
            lines.push_back(std::move(currentLine));
            currentLine = MakeLine(lines.back().y + lines.back().height);
            cursorX = startX;
            gap     = 0;
        }

        // --- NEW: Handle Replaced Image element layout packing ---
        if (w.isImage) {
            LayoutBox run;
            run.kind   = BoxKind::TextRun; // Fits your existing enum; handled contextually by tag
            run.x      = cursorX + gap;
            run.y      = currentLine.y; // Placeholder default
            run.width  = w.width;
            run.height = w.height; // Sets custom bounds! pushes maxH up automatically in FinalizeLineMetrics
            run.node   = w.node;
            run.text   = "";

            currentLine.children.push_back(std::move(run));
            cursorX += gap + w.width;
            continue;
        }

        // 2. Handle Text Overflow / Truncation (When nowrap is active and we hit the wall)
        if (isNoWrap && cursorX + gap + w.width > rightEdge) {
            if (doEllipsis) {
                // We need to fit what we can of this word, plus the "..."
                Font* font = nullptr;
                lr_.PrepareFontContext(s, w.fontSize, font);
                int ellipsisWidth = font->GetGlyph('.').advance * 3;

                std::string truncatedText = w.text;
                int availableWidth = rightEdge - (cursorX + gap) - ellipsisWidth;

                // Remeasure character by character backwards until it fits
                while (!truncatedText.empty() && availableWidth > 0) {
                    int currentW = 0;
                    // Custom simplified character measurement loop
                    for (char c : truncatedText) {
                        currentW += font->GetGlyph(c).advance;
                    }
                    if (currentW <= availableWidth) {
                        break;
                    }
                    truncatedText.pop_back();
                }

                truncatedText += "...";
                int finalWidth = 0;
                for (char c : truncatedText) finalWidth += font->GetGlyph(c).advance;

                // Emit the truncated TextRun
                LayoutBox run;
                run.kind     = BoxKind::TextRun;
                run.x        = cursorX + gap;
                run.y        = currentLine.y;
                run.width    = finalWidth;
                run.height   = base.lineHeight;
                run.fontSize = w.fontSize;
                run.node     = w.node;
                run.text     = truncatedText;

                currentLine.children.push_back(std::move(run));
            }

            // Break early! We hit the max-width barrier on a nowrap container.
            break;
        }

        // 3. Normal positioning path (Everything fits safely)
        cursorX += gap;

        Font* font = nullptr;
        FontMetrics wm = lr_.PrepareFontContext(s, w.fontSize, font);

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
            FinalizeLineMetrics(currentLine, lr_);
            lines.push_back(std::move(currentLine));
        }

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

        int nextY = startY;
        for (auto& line : lines) {
            nextY = line.y + line.height;
            parent.children.push_back(std::move(line));
        }
        return nextY;
    }

    int Layout(const Node&, LayoutBox&, int, int contentY, int) override {
        return contentY;
    }

private:
    static void FinalizeLineMetrics(LayoutBox& line, LayoutRenderer& lr) {
        int maxAscent = 0;
        int maxDescent = 0;
        int maxH = 0;

        // --- Pass 1: Gather overall line metrics from all children ---
        for (const auto& run : line.children) {
            maxH = std::max(maxH, run.height);

            if (run.node) {
                if (run.node->tag == "img") {
                    // An inline image sitting on the baseline acts entirely as an "ascent"
                    // block for the line box metrics.
                    maxAscent = std::max(maxAscent, run.height);
                } else {
                    const Style& style = (run.node->type == NodeType::Text && run.node->parent)
                                         ? run.node->parent->computedStyle
                                         : run.node->computedStyle;

                    Font* font = nullptr;
                    FontMetrics wm = lr.PrepareFontContext(style, run.fontSize, font);

                    maxAscent  = std::max(maxAscent, wm.ascent);
                    maxDescent = std::max(maxDescent, wm.descent);
                }
            }
        }

        // Adjust maxH if the combined text ascent/descent exceeds the tallest single item
        maxH = std::max(maxH, maxAscent + maxDescent);

        // Fallback to standard font size metrics if the line is completely empty
        if (maxH == 0) {
            maxH = lr.BaseFont.GetMetrics().lineHeight;
            maxAscent = lr.BaseFont.GetMetrics().ascent;
            maxDescent = lr.BaseFont.GetMetrics().descent;
        }

        line.height = maxH;
        line.lineAscent  = maxAscent;
        line.lineDescent = maxDescent;

        // --- Pass 2: Position elements vertically based on their parsed enum ---
        for (auto& run : line.children) {
            if (!run.node) continue;

            const Style& style = (run.node->type == NodeType::Text && run.node->parent)
                                 ? run.node->parent->computedStyle
                                 : run.node->computedStyle;

            // Resolve local font metrics for text offset calculations
            Font* font = nullptr;
            FontMetrics wm = lr.PrepareFontContext(style, run.fontSize, font);

            switch (style.verticalAlign) {
                case VerticalAlign::Top:
                    // Align top of element box with top of the entire line box
                    run.y = line.y;
                    break;

                case VerticalAlign::Bottom:
                    // Align bottom of element box with bottom of the entire line box
                    run.y = line.y + line.height - run.height;
                    break;

                case VerticalAlign::Middle: {
                    // Align vertical midpoint of the element box with line baseline + half x-height
                    // A reliable layout proxy for x-height is roughly half the structural ascent
                    int lineXHeight = maxAscent / 2;
                    int lineMidpointY = line.y + maxAscent - lineXHeight;
                    run.y = lineMidpointY - (run.height / 2);
                    break;
                }

                case VerticalAlign::Other: {
                    // Custom length/percentage offset shifts relative to the text baseline
                    // Negative goes down, positive goes up
                    int baselineY = line.y + maxAscent;
                    int customOffset = ResolveLength(style.verticalAlignValue, wm.lineHeight);

                    if (run.node->tag == "img") {
                        run.y = baselineY - run.height - customOffset;
                    } else {
                        run.y = baselineY - wm.ascent - customOffset;
                    }
                    break;
                }
                case VerticalAlign::TextTop:
                case VerticalAlign::TextBottom:
                case VerticalAlign::Super:
                case VerticalAlign::Sub: {
                    std::cerr << "VerticalAlign mode not implemented!" << std::endl;
                    break;
                }

                case VerticalAlign::Baseline:
                default: {
                    // Standard default baseline alignment path
                    int baselineY = line.y + maxAscent;
                    if (run.node->tag == "img") {
                        // Replaced element boxes sit directly on top of the text baseline
                        run.y = baselineY - run.height;
                    } else {
                        // Align the actual font structural baseline
                        run.y = baselineY - wm.ascent;
                    }
                    break;
                }
            }
    }
}

    LayoutRenderer& lr_;
    TextAlign       align_;
};

// ===========================================================================
//  BlockFormattingContext — normal block layout
// ===========================================================================

class BlockFormattingContext : public FormattingContext {
public:
    explicit BlockFormattingContext(LayoutRenderer& lr) : lr_(lr) {}

    int Layout(const Node& node, LayoutBox& parent, int contentX, int contentY, int contentWidth) override {
        int cursorY = contentY;
        const auto& kids = node.children;
        size_t i = 0;
        int prevMarginBottom = 0;

        bool hasBlockChildren = false;
        for (const auto& child : kids) {
            if (!child) continue; // FIX: Skip empty unique_ptrs safely
            if (!IsLayoutIgnored(*child) && !IsInlineChild(*child)) {
                hasBlockChildren = true;
                break;
            }
        }

        while (i < kids.size()) {
            const Node& child = *kids[i];
            if (IsLayoutIgnored(child)) { ++i; continue; }

            if (IsInlineChild(child)) {
                cursorY += prevMarginBottom;
                prevMarginBottom = 0;

                if (hasBlockChildren) {
                    LayoutBox anonBox;
                    anonBox.kind = BoxKind::Block;
                    anonBox.node = nullptr;
                    anonBox.x = contentX;
                    anonBox.y = cursorY;
                    anonBox.width = 0;

                    i = LayoutInlineRun(kids, i, anonBox, contentX, cursorY, contentWidth, node.computedStyle.textAlign, cursorY);

                    int maxLineRightEdge = contentX;
                    for (const auto& lineBox : anonBox.children) {
                        maxLineRightEdge = std::max(maxLineRightEdge, lineBox.x + lineBox.width);
                    }
                    anonBox.width = maxLineRightEdge - contentX;
                    anonBox.height = cursorY - anonBox.y;

                    parent.children.push_back(std::move(anonBox));
                } else {
                    i = LayoutInlineRun(kids, i, parent, contentX, cursorY, contentWidth, node.computedStyle.textAlign, cursorY);
                }
            } else {
                const auto& s = child.computedStyle;
                int marginTop = ResolveLength(s.margin_top, contentWidth);

                int collapsedMargin = std::max(prevMarginBottom, marginTop);
                cursorY += collapsedMargin;

                LayoutBox cb = lr_.LayoutBlock(child, contentX, cursorY, contentWidth);

                cursorY = cb.y + cb.height;
                prevMarginBottom = ResolveLength(s.margin_bottom, contentWidth);

                parent.children.push_back(std::move(cb));
                ++i;
            }
        }

        cursorY += prevMarginBottom;
        return cursorY;
    }

private:
    size_t LayoutInlineRun(const std::vector<std::unique_ptr<Node>>& kids, size_t start, LayoutBox& parent,
                            int contentX, int contentY, int contentWidth, TextAlign align, int& cursorY) {
        std::vector<const Node*> run;
        size_t j = start;

        while (j < kids.size()) {
            const Node& c = *kids[j];
            if (IsLayoutIgnored(c)) { ++j; continue; }
            if (!IsInlineChild(c)) break;

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

LayoutBox LayoutRenderer::LayoutBlock(const Node& node, int containerX, int containerY, int containerWidth) {
    const Style& s = node.computedStyle;

    LayoutBox box;
    box.kind = BoxKind::Block;
    box.node = &node;

    // --- 1. Compute Horizontal Frame Padding/Borders ---
    int paddingLeft   = ResolveLength(s.padding_left, containerWidth);
    int paddingRight  = ResolveLength(s.padding_right, containerWidth);
    int paddingX      = paddingLeft + paddingRight;

    int borderLeftWidth   = GetVisibleBorderWidth(s.BorderLeft);
    int borderRightWidth  = GetVisibleBorderWidth(s.BorderRight);
    int borderTopWidth    = GetVisibleBorderWidth(s.BorderTop);
    int borderBottomWidth = GetVisibleBorderWidth(s.BorderBottom);
    int borderX           = borderLeftWidth + borderRightWidth;

    // Change this flag: body shouldn't wrap-shrink by default!
    bool shrinkToFit = (node.tag == "span" || node.tag == "button");

    // --- 2. Resolve Outer Box Width ---
    bool hasExplicitWidth = (s.width.unit != LengthUnit::Auto);
    if (hasExplicitWidth) {
        int computedContentWidth = ResolveLength(s.width, containerWidth);
        if (s.boxSizing == BoxSizing::ContentBox) {
            box.width = computedContentWidth + paddingX + borderX;
        } else {
            // BoxSizing::BorderBox: The specified width is the total outer box width
            box.width = computedContentWidth;
        }
    } else {
        // Standard block tags occupy full parent context horizontal spans
        box.width = containerWidth;
    }

    // --- 3. Apply Horizontal Constraints (Keep existing max/min logic...)

    // --- 4. Position the Box Horizontally (Resolving Auto Margins) ---

    int marginLeft   = (s.margin_left.unit   == LengthUnit::Auto) ? 0 : ResolveLength(s.margin_left, containerWidth);
    int marginRight  = (s.margin_right.unit  == LengthUnit::Auto) ? 0 : ResolveLength(s.margin_right, containerWidth);

    // Vertical auto margins ALWAYS resolve to 0 in standard flow
    int marginTop    = (s.margin_top.unit    == LengthUnit::Auto) ? 0 : ResolveLength(s.margin_top, containerWidth);
    int marginBottom = (s.margin_bottom.unit == LengthUnit::Auto) ? 0 : ResolveLength(s.margin_bottom, containerWidth);

    int remainingSpace = containerWidth - box.width;
    if (remainingSpace > 0) {
        if (s.margin_left.unit == LengthUnit::Auto && s.margin_right.unit == LengthUnit::Auto) {
            marginLeft  = remainingSpace / 2;
            marginRight = remainingSpace - marginLeft;
        } else if (s.margin_left.unit == LengthUnit::Auto) {
            marginLeft  = remainingSpace - marginRight;
        } else if (s.margin_right.unit == LengthUnit::Auto) {
            marginRight = remainingSpace - marginLeft;
        }
    }

    box.x = containerX + marginLeft;
    // We will let the BlockFormattingContext calculate the exact box.y
    // to account for collapsing adjacent margins!
    box.y = containerY;

    // --- 5. Determine Inner Content Context ---
    int contentX     = box.x + borderLeftWidth + paddingLeft;
    int contentY     = box.y + borderTopWidth  + ResolveLength(s.padding_top, containerWidth);
    int contentWidth = std::max(0, box.width - paddingX - borderX);

    // --- 6. Choose Formatting Context & Layout Children ---
    std::unique_ptr<FormattingContext> ctx = std::make_unique<BlockFormattingContext>(*this);
    int endY = ctx->Layout(node, box, contentX, contentY, contentWidth);

    // --- 7. Recalculate Width ONLY if width is 'auto' AND shrinkToFit is true ---
    if (!hasExplicitWidth && shrinkToFit && !box.children.empty()) {
        int maxChildRightEdge = contentX;
        for (const auto& childBox : box.children) {
            maxChildRightEdge = std::max(maxChildRightEdge, childBox.x + childBox.width);
        }
        int calculatedContentWidth = maxChildRightEdge - contentX;
        box.width = calculatedContentWidth + paddingX + borderX;

        int finalSpace = containerWidth - box.width;
        if (finalSpace > 0 && s.margin_left.unit == LengthUnit::Auto && s.margin_right.unit == LengthUnit::Auto) {
            box.x = containerX + finalSpace / 2;
        }
    }

    // --- 8. Resolve Outer Box Height ---
    int paddingTop    = ResolveLength(s.padding_top, containerWidth);
    int paddingBottom = ResolveLength(s.padding_bottom, containerWidth);
    int paddingY      = paddingTop + paddingBottom;
    int borderY        = borderTopWidth + borderBottomWidth;

    if (s.height.unit != LengthUnit::Auto) {
        int computedContentHeight = ResolveLength(s.height, 0);
        if (s.boxSizing == BoxSizing::ContentBox) {
            box.height = computedContentHeight + paddingY + borderY;
        } else {
            // BoxSizing::BorderBox: The specified height is the total outer box height
            box.height = computedContentHeight;
        }
    } else {
        box.height = (endY - contentY) + paddingY + borderY;
    }

    if (s.max_height.unit != LengthUnit::Auto) box.height = std::min(box.height, ResolveLength(s.max_height, 0));
    if (s.min_height.unit != LengthUnit::Auto) box.height = std::max(box.height, ResolveLength(s.min_height, 0));

    return box;
}
// ---------------------------------------------------------------------------
//  DOM → layout root
// ---------------------------------------------------------------------------

void LayoutRenderer::Update(const Node& dom) {
    const Node* body = nullptr;
    std::function<const Node*(const Node&)> findBody = [&](const Node& n) -> const Node* {
        for (const auto& child : n.children) {
            if (child->tag == "body") return child.get();
            if (!IsLayoutIgnored(*child)) {
                if (auto* r = findBody(*child)) return r;
            }
        }
        return nullptr;
    };
    body = findBody(dom);
    assert(body && "DOM must contain a <body> element");

    int bodyMarginTop = ResolveLength(body->computedStyle.margin_top, renderer.GetWidth());
    root = LayoutBlock(*body, 0, bodyMarginTop, renderer.GetWidth());
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
            if (r.a != 0) return r;
        }
        return Color(0, 0, 0, 0);
    };
    Color bg = find(root);
    return bg.a != 0 ? bg : Color(255, 255, 255);
}

void LayoutRenderer::Render(const LayoutBox& box) {
    if (box.kind == BoxKind::Block) {
        RenderBlock(box);
    } else if (box.kind == BoxKind::Line) {
        // Line box itself doesn't hold text size. Find the maximum text size within its text run children!
        int activeLineFontSize = 16;
        for (const auto& run : box.children) {
            if (run.fontSize > 0) {
                activeLineFontSize = run.fontSize;
                break;
            }
        }
        RenderLine(box, activeLineFontSize);
    }

    if (box.kind == BoxKind::TextRun) {
        if (box.node && (box.node->tag == "img" || box.node->tag == "IMG" || box.node->imageData != nullptr)) {
            RenderImage(box);
        } else {
            RenderTextRun(box);
        }
    }

    for (const auto& child : box.children) {
        Render(child);
    }
}

void LayoutRenderer::Render() {
    renderer.Clear(Color(255,255,255));
    Render(root);
}

void LayoutRenderer::RenderBox(const LayoutBox& box) {
    switch (box.kind) {
        case BoxKind::Block:
            RenderBlock(box);
            break;

        case BoxKind::Line:
            RenderLine(box, box.fontSize);
            break;

        case BoxKind::TextRun:
            // FIX: Check for the presence of image data explicitly alongside the tag
            if (box.node && (box.node->tag == "img" || box.node->tag == "IMG" || box.node->imageData != nullptr)) {
                RenderImage(box);
            } else {
                RenderTextRun(box);
            }
            break;
    }

    for (const auto& child : box.children)
        RenderBox(child);
}
void LayoutRenderer::RenderImage(const LayoutBox& box) const {
    if (!box.node) return;

    // Check if the texture array has been resolved and loaded by the decoder thread
    if (box.node->imageData && box.node->imageData->isLoaded && !box.node->imageData->pixels.empty()) {
        const auto& imgData = *(box.node->imageData);
        int srcW = imgData.intrinsicWidth;
        int srcH = imgData.intrinsicHeight;

        // Loop over the allocated layout screen box dimensions
        for (int dy = 0; dy < box.height; ++dy) {
            for (int dx = 0; dx < box.width; ++dx) {
                // Map screen coordinate space back to texture coordinates
                int sx = (dx * srcW) / box.width;
                int sy = (dy * srcH) / box.height;

                // Protect against out-of-bounds rounding checks
                sx = std::clamp(sx, 0, srcW - 1);
                sy = std::clamp(sy, 0, srcH - 1);

                Color pixel = imgData.pixels[sy * srcW + sx];

                // Check alpha channel transparency threshold
                if (pixel.a == 0) continue;

                renderer.DrawPixel(box.x + dx, box.y + dy, pixel);
            }
        }
    } else {
        // Fallback: Render a standard grey bounding box placeholder while loading
        renderer.FillRect(box.x, box.y, box.width, box.height, Color(245, 245, 245));

        // Draw placeholder border frames matching your solid border thickness rules
        renderer.FillRect(box.x, box.y, box.width, 1, Color(200, 200, 200));
        renderer.FillRect(box.x, box.y + box.height - 1, box.width, 1, Color(200, 200, 200));
        renderer.FillRect(box.x, box.y, 1, box.height, Color(200, 200, 200));
        renderer.FillRect(box.x + box.width - 1, box.y, 1, box.height, Color(200, 200, 200));
    }
}
// ---------------------------------------------------------------------------

void LayoutRenderer::RenderTextRun(const LayoutBox& box) {
    assert(box.node && box.node->parent);
    const Style& s = box.node->parent ? box.node->parent->computedStyle : box.node->computedStyle;

    Font* font = nullptr;
    FontMetrics m = PrepareFontContext(s, box.fontSize, font);

    int baseline   = box.y + m.ascent;
    Color color    = s.color;

    int cursorX = box.x;
    char prev   = 0;
    for (char c : box.text) {
        if (prev) cursorX += font->GetKerning(c, prev).x >> 6;
        const Glyph& g = font->GetGlyph(c);
        renderer.DrawGlyph(cursorX + g.bearingX, baseline - g.bearingY, g, color);
        cursorX += g.advance;
        prev = c;
    }
}

// ---------------------------------------------------------------------------

void LayoutRenderer::RenderBlock(const LayoutBox& box) {
    if (!box.node) return;
    const Style& s = box.node->computedStyle;

    if (s.hasBackground) {
        renderer.FillRect(box.x, box.y, box.width, box.height, s.backgroundColor);
    }

    int borderLeftWidth   = GetVisibleBorderWidth(s.BorderLeft);
    int borderRightWidth  = GetVisibleBorderWidth(s.BorderRight);
    int borderTopWidth    = GetVisibleBorderWidth(s.BorderTop);
    int borderBottomWidth = GetVisibleBorderWidth(s.BorderBottom);

    if (borderTopWidth > 0) {
        RenderSingleBorderEdge(s.BorderTop, box.x, box.x + box.width, box.y, true);
    }

    if (borderBottomWidth > 0) {
        int bottomY = box.y + box.height - borderBottomWidth;
        RenderSingleBorderEdge(s.BorderBottom, box.x, box.x + box.width, bottomY, true);
    }

    if (borderLeftWidth > 0) {
        int startY = box.y + borderTopWidth;
        int endY   = box.y + box.height - borderBottomWidth;
        RenderSingleBorderEdge(s.BorderLeft, startY, endY, box.x, false);
    }

    if (borderRightWidth > 0) {
        int startY    = box.y + borderTopWidth;
        int endY      = box.y + box.height - borderBottomWidth;
        int fixedRegX = box.x + box.width - borderRightWidth;
        RenderSingleBorderEdge(s.BorderRight, startY, endY, fixedRegX, false);
    }
}

// ---------------------------------------------------------------------------

void LayoutRenderer::RenderSingleBorderEdge(const Border_side& edge, int start, int end, int fixedCoord, bool isHorizontal) {
    BorderStyle style = edge.borderStyle;
    Color color       = edge.borderColor;
    int thickness = GetVisibleBorderWidth(edge);


    if (style == BorderStyle::none || style == BorderStyle::hidden || thickness <= 0) {
        return;
    }

    switch (style) {
        case BorderStyle::solid: {
            if (isHorizontal) {
                renderer.FillRect(start, fixedCoord, end - start, thickness, color);
            } else {
                renderer.FillRect(fixedCoord, start, thickness, end - start, color);
            }
            break;
        }

        case BorderStyle::double_border: {
            int lineThickness = std::max(1, thickness / 3);
            int gap = std::max(1, thickness - (lineThickness * 2));

            if (isHorizontal) {
                renderer.FillRect(start, fixedCoord, end - start, lineThickness, color);
                renderer.FillRect(start, fixedCoord + lineThickness + gap, end - start, lineThickness, color);
            } else {
                renderer.FillRect(fixedCoord, start, lineThickness, end - start, color);
                renderer.FillRect(fixedCoord + lineThickness + gap, start, lineThickness, end - start, color);
            }
            break;
        }

        case BorderStyle::dotted: {
            int radius = std::max(1, thickness / 2);
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

            for (int pos = start; pos < end; pos += (dashLen + gapLen)) {
                int currentDashLen = std::min(dashLen, end - pos);
                if (isHorizontal) {
                    renderer.FillRect(pos, fixedCoord, currentDashLen, thickness, color);
                } else {
                    renderer.FillRect(fixedCoord, pos, thickness, currentDashLen, color);
                }
            }
            break;
        }
        default:
            break;
    }
}
void LayoutRenderer::RenderLine(const LayoutBox& box, int Text_Height) {
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
        thickness  = ResolveLength(s.TextDecorationThickness, Text_Height);
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

    RenderDecoration(decorStyle, decorColor, thickness, startX, endX, y);
}

// ---------------------------------------------------------------------------
// All decoration styles isolated here — add new ones without touching
// RenderLine().
void LayoutRenderer::RenderDecoration(
        TextDecorationStyle style, Color color, int thickness,
        int startX, int endX, int y)
{
    // Remove the style-reading lines that used to live here!

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