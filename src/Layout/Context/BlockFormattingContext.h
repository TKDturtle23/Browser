//
// Created by tkdtu on 5/31/2026.
//

#ifndef BROWSER_BLOCKFORMATTINGCONTEXT_H
#define BROWSER_BLOCKFORMATTINGCONTEXT_H
#include "FormattingContext.h"

struct BlockResult {
    LayoutBox box;
    // The bottom margin of the last in-flow child that "escaped" upward
    // because the box had no bottom border or padding. The parent BFC folds
    // this into its prevMarginBottom so it collapses correctly with the next
    // sibling's margin-top (CSS §8.3.1 parent-child margin collapsing).
    // Zero when the box has an explicit height, a bottom border, or padding.
    double escapedMarginBottom = 0;
};


class LayoutGenerator;

class BlockFormattingContext : public FormattingContext {
public:
    explicit BlockFormattingContext(LayoutGenerator& lr) : lr_(lr) {}

    int Layout(Node& node, LayoutBox& parent, int contentX, int contentY, int contentWidth, int contentHeight) override;
    int GetLastChildMarginBottom() const { return lastChildMarginBottom_; }
private:
    size_t LayoutInlineRun(std::vector<std::unique_ptr<Node>>& kids, size_t start,
                           LayoutBox& parent, int contentX, int contentY,
                           int contentWidth, TextAlign align, int& cursorY) const;
    int lastChildMarginBottom_ = 0;
    LayoutGenerator& lr_;
};


#endif //BROWSER_BLOCKFORMATTINGCONTEXT_H
