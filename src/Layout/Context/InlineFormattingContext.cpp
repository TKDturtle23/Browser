//
// Created by tkdtu on 5/31/2026.
//

#include "InlineFormattingContext.h"

#include <iostream>
#include "../LayoutGenerator.h"

int InlineFormattingContext::LayoutRoots(std::vector<Node *> &roots, LayoutBox &parent, int startX, int startY,
                                         int containerWidth) const {
    std::vector<Word> words;
    WordCollector wc(
        lr_.BaseFont, lr_.BaseItalicFont,
        lr_.BaseBoldFont, lr_.BaseBoldItalicFont,
        words,
        [&](const Style& s) -> Font& { return lr_.ResolveFont(s); }, lr_.GetWidth(), lr_.GetHeight()
    );
    for (Node* n : roots) wc.Visit(*n);

    Font& baseFont   = lr_.BaseFont;
    FontMetrics base = baseFont.GetMetrics();
    int spaceWidth   = baseFont.GetGlyph(' ').advance;
    int rightEdge    = startX + containerWidth;

    auto MakeLine = [&](int y) {
        LayoutBox line;
        line.kind   = BoxKind::Line;
        line.x      = startX;
        line.y      = y;
        line.width  = containerWidth;
        line.height = base.lineHeight;
        return line;
    };

    std::vector<LayoutBox> lines;
    LayoutBox currentLine = MakeLine(startY);
    int cursorX = startX;

    for (const Word& w : words) {
        const Style& s = (w.node->parent) ? w.node->parent->computedStyle : w.node->computedStyle;
        bool isNoWrap   = (s.whiteSpace == WhiteSpace::nowrap);
        bool doEllipsis = (s.textOverflow == TextOverflow::Ellipsis);

        bool lineHasContent = !currentLine.children.empty();
        int  gap            = (lineHasContent && w.hasSpaceBefore) ? spaceWidth : 0;

        // Line wrap
        if (!isNoWrap && lineHasContent && cursorX + gap + w.width > rightEdge) {
            FinalizeLineMetrics(currentLine, lr_);
            lines.push_back(std::move(currentLine));
            currentLine = MakeLine(lines.back().y + lines.back().height);
            cursorX = startX;
            gap     = 0;
        }

        // Inline image
        if (w.isImage) {
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

        // Overflow truncation (nowrap + ellipsis)
        if (isNoWrap && cursorX + gap + w.width > rightEdge) {
            if (doEllipsis) {
                Font* font = nullptr;
                lr_.PrepareFontContext(s, w.fontSize, font);
                int ellipsisWidth  = font->GetGlyph('.').advance * 3;
                int availableWidth = rightEdge - (cursorX + gap) - ellipsisWidth;

                std::string truncated = w.text;
                while (!truncated.empty()) {
                    int textW = 0;
                    for (char c : truncated) textW += font->GetGlyph(c).advance;
                    if (textW <= availableWidth) break;
                    truncated.pop_back();
                }
                truncated += "...";

                int finalWidth = 0;
                for (char c : truncated) finalWidth += font->GetGlyph(c).advance;

                LayoutBox run;
                run.kind     = BoxKind::TextRun;
                run.x        = cursorX + gap;
                run.y        = currentLine.y;
                run.width    = finalWidth;
                run.height   = base.lineHeight;
                run.fontSize = w.fontSize;
                run.node     = w.node;
                run.text     = truncated;
                currentLine.children.push_back(std::move(run));
            }
            break;
        }

        // Normal text run
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

    // Alignment offsets
    if (align_ != TextAlign::Left) {
        for (auto& line : lines) {
            int contentRight = line.children.back().x + line.children.back().width;
            int slack        = containerWidth - (contentRight - line.x);
            if (slack <= 0) continue;
            int offset = (align_ == TextAlign::Center) ? slack / 2 : slack;
            for (auto& run : line.children) run.x += offset;
        }
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

    for (const auto& run : line.children) {
        maxH = std::max(maxH, run.height);
        if (!run.node) continue;

        if (run.node->tag == "img") {
            maxAscent = std::max(maxAscent, run.height);
        } else {
            const Style& style = (run.node->type == NodeType::Text && run.node->parent)
                                     ? run.node->parent->computedStyle
                                     : run.node->computedStyle;
            Font* font = nullptr;
            FontMetrics wm = lr.PrepareFontContext(style, run.fontSize, font);
            maxAscent  = std::max(maxAscent,  wm.ascent);
            maxDescent = std::max(maxDescent, wm.descent);
        }
    }

    maxH = std::max(maxH, maxAscent + maxDescent);
    if (maxH == 0) {
        maxH       = lr.BaseFont.GetMetrics().lineHeight;
        maxAscent  = lr.BaseFont.GetMetrics().ascent;
        maxDescent = lr.BaseFont.GetMetrics().descent;
    }

    line.height      = maxH;
    line.lineAscent  = maxAscent;
    line.lineDescent = maxDescent;

    for (auto& run : line.children) {
        if (!run.node) continue;

        const Style& style = (run.node->type == NodeType::Text && run.node->parent)
                                 ? run.node->parent->computedStyle
                                 : run.node->computedStyle;
        Font* font = nullptr;
        FontMetrics wm = lr.PrepareFontContext(style, run.fontSize, font);

        int baselineY = line.y + maxAscent;
        bool isImg    = (run.node->tag == "img");

        switch (style.verticalAlign) {
            case VerticalAlign::Top:
                run.y = line.y;
                break;
            case VerticalAlign::Bottom:
                run.y = line.y + line.height - run.height;
                break;
            case VerticalAlign::Middle: {
                int lineXHeight    = maxAscent / 2;
                int lineMidpointY  = line.y + maxAscent - lineXHeight;
                run.y = lineMidpointY - (run.height / 2);
                break;
            }
            case VerticalAlign::Other: {
                int customOffset = ResolveLength(style.verticalAlignValue, wm.lineHeight, lr.GetWidth(), lr.GetHeight());
                run.y = isImg ? (baselineY - run.height - customOffset)
                            : (baselineY - wm.ascent - customOffset);
                break;
            }
            case VerticalAlign::TextTop:
            case VerticalAlign::TextBottom:
            case VerticalAlign::Super:
            case VerticalAlign::Sub:
                std::cerr << "VerticalAlign mode not implemented!\n";
                break;
            case VerticalAlign::Baseline:
            default:
                run.y = isImg ? (baselineY - run.height)
                            : (baselineY - wm.ascent);
                break;
        }
    }
}
