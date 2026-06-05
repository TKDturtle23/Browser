//
// Created by tkdtu on 5/31/2026.
//

#include "BlockFormattingContext.h"
#include "InlineFormattingContext.h"
#include "Layout/LayoutGenerator.h"

int BlockFormattingContext::Layout(Node &node, LayoutBox &parent, int contentX, int contentY, int contentWidth, int contentHeight) {
    int cursorY        = contentY;
    int prevMarginBottom = 0;
    auto& kids   = node.children;
    size_t i           = 0;

    bool hasBlockChildren = false;
    for (const auto& child : kids) {
        if (child && !IsLayoutIgnored(*child) && !IsInlineChild(*child)) {
            hasBlockChildren = true;
            break;
        }
    }

    while (i < kids.size()) {
        Node& child = *kids[i];
        if (IsLayoutIgnored(child)) { ++i; continue; }

        if (IsInlineChild(child)) {
            cursorY += prevMarginBottom;
            prevMarginBottom = 0;

            if (hasBlockChildren) {
                LayoutBox anonBox;
                anonBox.kind  = BoxKind::Block;
                anonBox.node  = nullptr;
                anonBox.x     = contentX;
                anonBox.y     = cursorY;
                anonBox.width = 0;

                i = LayoutInlineRun(kids, i, anonBox, contentX, cursorY, contentWidth,
                                    node.computedStyle.textAlign, cursorY);

                int maxRight = contentX;
                for (const auto& lb : anonBox.children)
                    maxRight = std::max(maxRight, lb.x + lb.width);
                anonBox.width  = maxRight - contentX;
                anonBox.height = cursorY - anonBox.y;
                parent.children.push_back(std::move(anonBox));
            } else {
                i = LayoutInlineRun(kids, i, parent, contentX, cursorY, contentWidth,
                                    node.computedStyle.textAlign, cursorY);
            }
        } else {
            const auto& s       = child.computedStyle;
            int marginTop       = ResolveLength(s.margin_top, contentWidth, lr_.GetWidth(), lr_.GetHeight());
            int collapsedMargin = std::max(prevMarginBottom, marginTop);
            cursorY += collapsedMargin;

            LayoutBox cb = lr_.LayoutBlock(child, contentX, cursorY, contentWidth, contentHeight);
            cursorY      = cb.y + cb.height;
            prevMarginBottom = ResolveLength(s.margin_bottom, contentWidth, lr_.GetWidth(), lr_.GetHeight());

            parent.children.push_back(std::move(cb));
            ++i;
        }
    }

    cursorY += prevMarginBottom;
    return cursorY;
}

size_t BlockFormattingContext::LayoutInlineRun(std::vector<std::unique_ptr<Node>> &kids, size_t start,
    LayoutBox &parent, int contentX, int contentY, int contentWidth, TextAlign align, int &cursorY) const {
    std::vector<Node*> run;
    size_t j = start;
    while (j < kids.size()) {
        Node& c = *kids[j];
        if (IsLayoutIgnored(c))  { ++j; continue; }
        if (!IsInlineChild(c))   break;
        run.push_back(&c);
        ++j;
    }

    if (!run.empty()) {
        InlineFormattingContext ifc(lr_, align);
        cursorY = ifc.LayoutRoots(run, parent, contentX, contentY, contentWidth);
    }
    return j;
}
