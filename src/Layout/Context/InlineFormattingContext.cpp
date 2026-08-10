//
// Created by tkdtu on 5/31/2026.
//

#include "InlineFormattingContext.h"

#include <iostream>

#include "FontManager.h"
#include "../LayoutGenerator.h"
#include "../LayoutHelper.h"


// ---------------------------------------------------------------------------
// ResolveCharFont
//   Returns the best font for a single codepoint, walking the fallback chain:
//     1. primary font (f)
//     2. fallback Primary face
//     3. fallback Symbol face
//     4. fallback Emoji face
//   The returned pointer is always non-null (falls back to `primary` if nothing
//   claims the glyph).
// ---------------------------------------------------------------------------
static Font* ResolveCharFont(char32_t c, Font* primary) {
    if (primary->HasSymbol(c))
        return primary;

    FallbackFonts* fb = FontManager::getFallbackFont();

    if (fb->Primary.HasSymbol(c))  return &fb->Primary;
    if (fb->Symbol.HasSymbol(c))   return &fb->Symbol;
    if (fb->Emoji.HasSymbol(c))    return &fb->Emoji;

    return primary; // best-effort: render with primary anyway
}

// ---------------------------------------------------------------------------
// MeasureWord
//   Re-measures a word using per-character font fallback.
//   Useful when Word::width was computed before the fallback system existed
//   (or to verify correctness of inline width accounting).
// ---------------------------------------------------------------------------
static int MeasureWord(const std::u32string& text, Font* primary, IRenderBackend* rb) {
    int total = 0;
    for (char32_t c : text) {
        Font* f = ResolveCharFont(c, primary);
        f->SetSize(rb, primary->GetCurrentSize());
        total += f->GetGlyph(rb, c).advance;
    }
    return total;
}

// ---------------------------------------------------------------------------
// A lightweight sub-run produced during word decomposition:
//   one contiguous span of characters that all share the same font pointer.
// ---------------------------------------------------------------------------
struct CharRun {
    Font*          font      = nullptr;
    std::u32string text;
    int            width     = 0;   // sum of advance widths
    int            x         = 0;   // filled in during placement
};

// ---------------------------------------------------------------------------
// DecomposeWord
//   Splits a word's text into one or more CharRuns, grouping adjacent
//   characters that resolve to the same font.
// ---------------------------------------------------------------------------
static std::vector<CharRun> DecomposeWord(
    const std::u32string& text,
    Font*                 primary,
    IRenderBackend*       rb)
{
    std::vector<CharRun> runs;

    for (char32_t c : text) {
        Font* f = ResolveCharFont(c, primary);
        f->SetSize(rb, primary->GetCurrentSize());
        int adv = f->GetGlyph(rb, c).advance;

        if (!runs.empty() && runs.back().font == f) {
            runs.back().text  += c;
            runs.back().width += adv;
        } else {
            CharRun cr;
            cr.font  = f;
            cr.text += c;
            cr.width = adv;
            runs.push_back(std::move(cr));
        }
    }

    return runs;
}


int InlineFormattingContext::LayoutRoots(std::vector<Node *> &roots, LayoutBox &parent, int startX, int startY,
                                         int containerWidth, int containerHeight) const {
    std::vector<Word> words;
    auto group = FontManager::GetFontGroup("Arial");
    WordCollector wc(
        group.base, group.italic,
        group.bold, group.boldItalic,
        words,
        [&](const Style& s) -> Font& { return FontManager::ResolveFont(s); }, lr_.GetWidth(), lr_.GetHeight(), *FontManager::getFallbackFont()
    );
    for (Node* n : roots) wc.Visit(*n);

    std::shared_ptr<Font> baseFont   = group.base;
    FontMetrics base = baseFont->GetMetrics();
    int spaceWidth   = baseFont->GetGlyph(IRenderBackend::GetRenderBackend().get(), ' ').advance;
    int rightEdge    = startX + containerWidth;

    IRenderBackend* rb = IRenderBackend::GetRenderBackend().get();

    auto MakeLine = [&](int y) {
        LayoutBox line;
        line.kind   = BoxKind::Line;
        line.x      = startX;
        line.y      = y;
        line.width  = containerWidth;
        line.height = base.lineHeight;
        return line;
    };

    // -----------------------------------------------------------------------
    // EmitCharRuns
    //   Converts a list of CharRuns into LayoutBox children on currentLine,
    //   starting at xOffset.  Returns the new cursorX.
    // -----------------------------------------------------------------------
    auto EmitCharRuns = [&](
        std::vector<CharRun>& charRuns,
        Node*                 node,
        int                   fontSize,
        Font*                 primaryFont,
        LayoutBox&            line,
        int                   xOffset) -> int
    {
        int cx = xOffset;
        FontMetrics wm = primaryFont->GetMetrics(); // line height comes from the primary/styled font

        for (auto& cr : charRuns) {
            if (cr.text.empty()) continue;

            LayoutBox run;
            run.kind     = BoxKind::TextRun;
            run.x        = cx;
            run.y        = line.y;
            run.width    = cr.width;
            run.height   = wm.lineHeight;
            run.fontSize = fontSize;
            run.node     = node;
            run.text     = cr.text;
            // Store the resolved font so the renderer can use it directly.
            // If LayoutBox doesn't carry a font pointer yet, add:
            //   Font* resolvedFont = nullptr;
            // to its declaration and set it here.
            run.resolvedFont = cr.font;

            line.children.push_back(std::move(run));
            cx += cr.width;
        }
        return cx;
    };

    // -----------------------------------------------------------------------
    // Pending-run state
    //   We still batch words that share the same node/primaryFont/fontSize
    //   before emitting, but we no longer merge them into a single text blob;
    //   instead we keep an accumulated list of CharRuns so the space between
    //   two words of the same "run" is handled correctly.
    // -----------------------------------------------------------------------
    std::vector<CharRun> pendingCharRuns;
    Node*   pendingNode     = nullptr;
    Font*   pendingFont     = nullptr;   // primary/styled font (not fallback)
    int     pendingFontSize = 0;
    int     pendingX        = startX;
    int     pendingWidth    = 0;

    std::vector<LayoutBox> lines;
    LayoutBox currentLine = MakeLine(startY);
    int cursorX = startX;

    auto FlushPendingRun = [&](int lineY) {
        if (pendingCharRuns.empty()) return;
        EmitCharRuns(pendingCharRuns, pendingNode, pendingFontSize, pendingFont, currentLine, pendingX);
        pendingCharRuns.clear();
        pendingNode     = nullptr;
        pendingFont     = nullptr;
        pendingFontSize = 0;
        pendingWidth    = 0;
    };

    for (const Word& w : words) {

        const Style& s = (w.node->parent)
            ? w.node->parent->computedStyle
            : w.node->computedStyle;

        bool isNoWrap   = (s.whiteSpace == WhiteSpace::NoWrap);
        bool doEllipsis = (s.textOverflow == TextOverflow::Ellipsis);

        bool lineHasContent =
            !currentLine.children.empty() || !pendingCharRuns.empty();

        int gap = (lineHasContent && w.hasSpaceBefore)
            ? spaceWidth
            : 0;

        // Re-measure the word with fallback fonts so width is accurate
        Font* primaryFont = nullptr;
        FontMetrics wm = FontManager::PrepareFontContext(s, w.fontSize, primaryFont, lr_.GetWidth(), lr_.GetHeight());
        int wordWidth = MeasureWord(w.text, primaryFont, rb);

        // ------------------------------------------------------------------
        // Wrap
        // ------------------------------------------------------------------
        if (!isNoWrap &&
            lineHasContent &&
            cursorX + gap + wordWidth > rightEdge) {

            FlushPendingRun(currentLine.y);

            FinalizeLineMetrics(currentLine, lr_);
            lines.push_back(std::move(currentLine));

            currentLine = MakeLine(lines.back().y + lines.back().height);

            cursorX = startX;
            gap = 0;
        }

        // ------------------------------------------------------------------
        // Images — flush text first, then emit a single image box
        // ------------------------------------------------------------------
        if (w.isImage) {
            FlushPendingRun(currentLine.y);

            LayoutBox run;
            run.kind   = BoxKind::TextRun;
            run.x      = cursorX + gap;
            run.y      = currentLine.y;
            run.width  = w.width;
            run.height = w.height;
            run.node   = w.node;

            currentLine.children.push_back(std::move(run));

            cursorX += gap + w.width;
            continue;
        }

        // ------------------------------------------------------------------
        // nowrap + ellipsis — truncate with fallback-aware measurement
        // ------------------------------------------------------------------
        if (isNoWrap &&
            cursorX + gap + wordWidth > rightEdge) {

            FlushPendingRun(currentLine.y);

            if (doEllipsis) {
                // Measure "..." using the primary font (ellipsis is ASCII, always
                // in the primary face).
                int dotAdv        = primaryFont->GetGlyph(rb, '.').advance;
                int ellipsisWidth = dotAdv * 3;
                int available     = rightEdge - (cursorX + gap) - ellipsisWidth;

                std::u32string truncated = w.text;

                // Trim characters until the text fits the available width.
                while (!truncated.empty()) {
                    int textW = 0;
                    for (char32_t c : truncated) {
                        Font* cf = ResolveCharFont(c, primaryFont);
                        cf->SetSize(rb, primaryFont->GetCurrentSize());
                        textW += cf->GetGlyph(rb, c).advance;
                    }
                    if (textW <= available) break;
                    truncated.pop_back();
                }

                truncated += Utf8ToUtf32("...");

                // Decompose the truncated text into font-fallback sub-runs.
                auto charRuns = DecomposeWord(truncated, primaryFont, rb);

                // Measure total width for the LayoutBox.
                int finalWidth = 0;
                for (auto& cr : charRuns) finalWidth += cr.width;

                // Emit each sub-run as its own LayoutBox.
                EmitCharRuns(charRuns, w.node, w.fontSize, primaryFont, currentLine, cursorX + gap);
            }

            break;
        }

        // ------------------------------------------------------------------
        // Normal word — decompose into per-font CharRuns and batch or flush
        // ------------------------------------------------------------------
        auto charRuns = DecomposeWord(w.text, primaryFont, rb);

        bool needsNewRun =
            pendingCharRuns.empty() ||
            pendingNode     != w.node ||
            pendingFont     != primaryFont ||
            pendingFontSize != w.fontSize;

        if (needsNewRun) {
            FlushPendingRun(currentLine.y);

            pendingNode     = w.node;
            pendingFont     = primaryFont;
            pendingFontSize = w.fontSize;
            pendingX        = cursorX + gap;
            pendingWidth    = wordWidth;

            pendingCharRuns = std::move(charRuns);
        } else {
            // Same styled run: append a space sub-run if needed, then the
            // new word's char-runs (merging with the last if same font).
            if (gap > 0) {
                // Space always uses the primary font.
                if (!pendingCharRuns.empty() && pendingCharRuns.back().font == primaryFont) {
                    pendingCharRuns.back().text  += U' ';
                    pendingCharRuns.back().width += gap;
                } else {
                    CharRun spaceRun;
                    spaceRun.font  = primaryFont;
                    spaceRun.text  = U" ";
                    spaceRun.width = gap;
                    pendingCharRuns.push_back(std::move(spaceRun));
                }
                pendingWidth += gap;
            }

            // Merge new char-runs, combining with last pending run where
            // the font matches.
            for (auto& cr : charRuns) {
                if (!pendingCharRuns.empty() && pendingCharRuns.back().font == cr.font) {
                    pendingCharRuns.back().text  += cr.text;
                    pendingCharRuns.back().width += cr.width;
                } else {
                    pendingCharRuns.push_back(std::move(cr));
                }
            }
            pendingWidth += wordWidth;
        }

        cursorX += gap + wordWidth;
    }

    FlushPendingRun(currentLine.y);

    if (!currentLine.children.empty()) {
        FinalizeLineMetrics(currentLine, lr_);
        lines.push_back(std::move(currentLine));
    }

    // -----------------------------------------------------------------------
    // Alignment offsets
    // -----------------------------------------------------------------------
    if (align_ != TextAlign::Left) {
        unsigned long long SmallestOffset = 9999999999;
        for (auto& line : lines) {
            if (line.children.empty()) continue;

            int contentLeft  = line.children.front().x;
            int contentRight = line.children.back().x + line.children.back().width;
            int contentWidth = contentRight - contentLeft;
            int slack        = containerWidth - contentWidth;
            if (slack <= 0) continue;

            int offset = (align_ == TextAlign::Center) ? slack / 2 : slack;
            SmallestOffset = std::min(SmallestOffset, static_cast<unsigned long long>(offset));
            line.x += offset;

            for (auto& run : line.children)
                run.x += offset;
        }
        parent.TextCenteringOffset = SmallestOffset;
    }

    int nextY = startY;
    for (auto& line : lines) {
        nextY = line.y + line.height;
        parent.children.push_back(std::move(line));
    }
    return nextY;
}

void InlineFormattingContext::FinalizeLineMetrics(LayoutBox &line, LayoutGenerator &lr) {
    int maxAscent = 0, maxDescent = 0, maxH = 0;

    int minX = line.x;
    int maxX = line.x;
    bool hasChildren = !line.children.empty();

    if (hasChildren) {
        minX = line.children.front().x;
        maxX = line.children.front().x + line.children.front().width;
    }

    for (const auto& run : line.children) {
        maxH = std::max(maxH, run.height);

        minX = std::min(minX, run.x);
        maxX = std::max(maxX, run.x + run.width);

        if (!run.node) continue;

        if (run.node->tag == "img") {
            maxAscent = std::max(maxAscent, run.height);
        } else {
            const Style& style = (run.node->type == NodeType::Text && run.node->parent)
                                     ? run.node->parent->computedStyle
                                     : run.node->computedStyle;
            Font* font = nullptr;
            FontMetrics wm = FontManager::PrepareFontContext(style, run.fontSize, font, lr.GetWidth(), lr.GetHeight());
            maxAscent  = std::max(maxAscent,  wm.ascent);
            maxDescent = std::max(maxDescent, wm.descent);
        }
    }

    auto group = FontManager::GetFontGroup("Arial");
    maxH = std::max(maxH, maxAscent + maxDescent);
    if (maxH == 0) {
        maxH       = group.base->GetMetrics().lineHeight;
        maxAscent  = group.base->GetMetrics().ascent;
        maxDescent = group.base->GetMetrics().descent;
    }

    if (hasChildren) {
        line.x     = minX;
        line.width = maxX - minX;
    } else {
        line.width = 0;
    }

    line.height      = maxH;
    line.lineAscent  = maxAscent;
    line.lineDescent = maxDescent;

    for (auto& run : line.children) {
        if (!run.node) continue;

        const Style& style = (run.node->type == NodeType::Text && run.node->parent)
                                 ? run.node->parent->computedStyle
                                 : run.node->computedStyle;

        // Use the run's resolved font for metrics where available; fall back
        // to the styled font so vertical alignment is still style-driven.
        Font* styledFont = nullptr;
        FontMetrics wm = FontManager::PrepareFontContext(style, run.fontSize, styledFont, lr.GetWidth(), lr.GetHeight());

        int baselineY = line.y + maxAscent;
        bool isImg    = (run.node->tag == "img");

        switch (style.verticalAlign) {
            case VerticalAlignKeyword::Top:
                run.y = line.y;
                break;
            case VerticalAlignKeyword::Bottom:
                run.y = line.y + line.height - run.height;
                break;
            case VerticalAlignKeyword::Middle: {
                int lineXHeight   = maxAscent / 2;
                int lineMidpointY = line.y + maxAscent - lineXHeight;
                run.y = lineMidpointY - (run.height / 2);
                break;
            }
            case VerticalAlignKeyword::Other: {
                int customOffset = ResolveLength(style.verticalAlignValue, wm.lineHeight, lr.GetWidth(), lr.GetHeight(), run.fontSize);
                run.y = isImg ? (baselineY - run.height - customOffset)
                              : (baselineY - wm.ascent  - customOffset);
                break;
            }
            case VerticalAlignKeyword::TextTop:
            case VerticalAlignKeyword::TextBottom:
            case VerticalAlignKeyword::Super:
            case VerticalAlignKeyword::Sub:
                std::cerr << "VerticalAlign mode not implemented!\n";
                break;
            case VerticalAlignKeyword::Baseline:
            default:
                run.y = isImg ? (baselineY - run.height)
                              : (baselineY - wm.ascent);
                break;
        }
    }
}