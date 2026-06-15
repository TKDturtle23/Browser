//
// Created by tkdtu on 5/31/2026.
//

#include "BlockFormattingContext.h"
#include "InlineFormattingContext.h"
#include "Layout/LayoutGenerator.h"

int BlockFormattingContext::Layout(Node &node, LayoutBox &parent, int contentX, int contentY, int contentWidth, int contentHeight) {
    int cursorY          = contentY;
    int prevMarginBottom = 0;
    auto& kids           = node.children;
    size_t i             = 0;

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
            // Anonymous boxes carry no margins — flush prevMarginBottom directly.
            cursorY += prevMarginBottom;
            prevMarginBottom = 0;

            if (hasBlockChildren) {
                LayoutBox anonBox;
                anonBox.kind   = BoxKind::Block;
                anonBox.node   = nullptr;
                anonBox.x      = contentX;
                anonBox.y      = cursorY;
                anonBox.width  = contentWidth;

                i = LayoutInlineRun(kids, i, anonBox, contentX, cursorY, contentWidth, contentHeight,
                                    node.computedStyle.textAlign, cursorY);

                anonBox.height = std::max(0, cursorY - anonBox.y);
                parent.children.push_back(std::move(anonBox));
            } else {
                i = LayoutInlineRun(kids, i, parent, contentX, cursorY, contentWidth, contentHeight,
                                    node.computedStyle.textAlign, cursorY);
            }
        } else {
            const auto& s = child.computedStyle;

            int resolved = ResolveFontSizeInherit(&child, lr_.GetWidth(), lr_.GetHeight());

            int marginTop = ResolveLength(
                s.margin_top,
                contentWidth,
                lr_.GetWidth(),
                lr_.GetHeight(),
                resolved
            );

            // Collapse the previous block's bottom margin with this block's top margin.
            // Also fold in any margin that escaped upward out of the previous child
            // (i.e. the previous child had no bottom border/padding, so its last
            // descendant's margin bubbled out). prevMarginBottom already holds the
            // max of the previous child's own margin_bottom and its escaped margin,
            // so the three-way collapse is just a single std::max here.
            int collapsedMargin = std::max(prevMarginBottom, marginTop);
            cursorY += collapsedMargin;

            LayoutResult result = lr_.LayoutBlock(child, contentX, cursorY, contentWidth, contentHeight);
            cursorY = result.box.y + result.box.height;

            // If the child had no bottom edge (border + padding == 0), its trailing
            // margin escaped upward. Carry it forward as prevMarginBottom so it gets
            // collapsed with the next sibling's margin_top (or the parent's own
            // trailing margin at the end of this loop).
            prevMarginBottom = result.escapedMarginBottom;

            parent.children.push_back(std::move(result.box));
            ++i;
        }
    }

    // Store the final trailing margin. If this BFC's container has no bottom
    // border/padding, LayoutBlock will add it to escapedMarginBottom so the
    // grandparent BFC can collapse it further.
    lastChildMarginBottom_ = prevMarginBottom;
    return cursorY;  // does NOT include the trailing margin
}

size_t BlockFormattingContext::LayoutInlineRun(std::vector<std::unique_ptr<Node>> &kids, size_t start,
    LayoutBox &parent, int contentX, int contentY, int contentWidth, int contentHeight, TextAlign align, int &cursorY) const {
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
        cursorY = ifc.LayoutRoots(run, parent, contentX, contentY, contentWidth, contentHeight);
    }
    return j;
}