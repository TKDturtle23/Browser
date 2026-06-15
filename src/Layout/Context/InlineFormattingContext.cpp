//
// Created by tkdtu on 5/31/2026.
//

#include "InlineFormattingContext.h"

#include <iostream>

#include "FontManager.h"
#include "../LayoutGenerator.h"
#include "../LayoutHelper.h"


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

    std::u32string pendingText;
    Node* pendingNode = nullptr;
    const Style* pendingStyle = nullptr;
    Font* pendingFont = nullptr;
    int pendingFontSize = 0;
    int pendingX = startX;
    int pendingWidth = 0;

    auto FlushPendingRun = [&](int y) {
    if (pendingText.empty()) return;

    FontMetrics wm = pendingFont->GetMetrics();

    LayoutBox run;
    run.kind     = BoxKind::TextRun;
    run.x        = pendingX;
    run.y        = y;
    run.width    = pendingWidth;
    run.height   = wm.lineHeight;
    run.fontSize = pendingFontSize;
    run.node     = pendingNode;
    run.text     = pendingText;

    currentLine.children.push_back(std::move(run));

    pendingText.clear();
    pendingNode = nullptr;
    pendingStyle = nullptr;
    pendingFont = nullptr;
    pendingWidth = 0;
};

for (const Word& w : words) {



    const Style& s = (w.node->parent)
        ? w.node->parent->computedStyle
        : w.node->computedStyle;

    bool isNoWrap   = (s.whiteSpace == WhiteSpace::NoWrap);
    bool doEllipsis = (s.textOverflow == TextOverflow::Ellipsis);

    bool lineHasContent =
        !currentLine.children.empty() || !pendingText.empty();

    int gap = (lineHasContent && w.hasSpaceBefore)
        ? spaceWidth
        : 0;

    // Wrap
    if (!isNoWrap &&
        lineHasContent &&
        cursorX + gap + w.width > rightEdge) {

        FlushPendingRun(currentLine.y);

        FinalizeLineMetrics(currentLine, lr_);
        lines.push_back(std::move(currentLine));

        currentLine = MakeLine(lines.back().y + lines.back().height);

        cursorX = startX;
        gap = 0;
    }

    // Images always flush text first
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
    Font* font = nullptr;
    FontMetrics wm = FontManager::PrepareFontContext(
        s,
        w.fontSize,
        font,
        lr_.GetWidth(),
        lr_.GetHeight()
    );

    // nowrap + ellipsis
    if (isNoWrap &&
        cursorX + gap + w.width > rightEdge) {

        FlushPendingRun(currentLine.y);

        if (doEllipsis) {
            int ellipsisWidth =
                font->GetGlyph(
                    IRenderBackend::GetRenderBackend().get(),
                    '.'
                ).advance * 3;

            int availableWidth =
                rightEdge - (cursorX + gap) - ellipsisWidth;

            std::u32string truncated = w.text;

            while (!truncated.empty()) {
                int textW = 0;

                for (char32_t c : truncated) {
                    textW += font->GetGlyph(
                        IRenderBackend::GetRenderBackend().get(),
                        c
                    ).advance;
                }

                if (textW <= availableWidth)
                    break;

                truncated.pop_back();
            }

            truncated += Utf8ToUtf32("...");

            int finalWidth = 0;

            for (char32_t c : truncated) {
                finalWidth += font->GetGlyph(
                    IRenderBackend::GetRenderBackend().get(),
                    c
                ).advance;
            }

            LayoutBox run;
            run.kind     = BoxKind::TextRun;
            run.x        = cursorX + gap;
            run.y        = currentLine.y;
            run.width    = finalWidth;
            run.height   = wm.lineHeight;
            run.fontSize = w.fontSize;
            run.node     = w.node;
            run.text     = truncated;

            currentLine.children.push_back(std::move(run));
        }

        break;
        }
    // Start new buffered run if needed
    bool needsNewRun =
        pendingText.empty() ||
        pendingNode != w.node ||
        pendingFont != font ||
        pendingFontSize != w.fontSize;

    if (needsNewRun) {
        FlushPendingRun(currentLine.y);

        pendingText = w.text;
        pendingNode = w.node;
        pendingStyle = &s;
        pendingFont = font;
        pendingFontSize = w.fontSize;

        pendingX = cursorX + gap;
        pendingWidth = w.width;
    } else {
        if (gap) {
            pendingText += ' ';
            pendingWidth += gap;
        }

        pendingText += w.text;
        pendingWidth += w.width;
    }

    cursorX += gap + w.width;
}

FlushPendingRun(currentLine.y);

    if (!currentLine.children.empty()) {
        FinalizeLineMetrics(currentLine, lr_);
        lines.push_back(std::move(currentLine));
    }
    // Alignment offsets
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


            for (auto& run : line.children) {
                run.x += offset;

            }
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

    // Track bounds for shrink-to-fit calculation
    int minX = line.x;
    int maxX = line.x;
    bool hasChildren = !line.children.empty();

    if (hasChildren) {
        minX = line.children.front().x;
        maxX = line.children.front().x + line.children.front().width;
    }

    for (const auto& run : line.children) {
        maxH = std::max(maxH, run.height);

        // Expand bounding box tracking to fit every child run
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

    // SHRINK TO FIT:
    // Set the line x and width strictly based on the bounding bounds of its actual text/image runs.
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
        Font* font = nullptr;
        FontMetrics wm = FontManager::PrepareFontContext(style, run.fontSize, font, lr.GetWidth(), lr.GetHeight());

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
                int lineXHeight    = maxAscent / 2;
                int lineMidpointY  = line.y + maxAscent - lineXHeight;
                run.y = lineMidpointY - (run.height / 2);
                break;
            }
            case VerticalAlignKeyword::Other: {
                int customOffset = ResolveLength(style.verticalAlignValue, wm.lineHeight, lr.GetWidth(), lr.GetHeight(), run.fontSize);
                run.y = isImg ? (baselineY - run.height - customOffset)
                            : (baselineY - wm.ascent - customOffset);
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
