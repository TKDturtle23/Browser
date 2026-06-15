//
// Created by tkdtu on 5/31/2026.
//

#ifndef BROWSER_BLOCKFORMATTINGCONTEXT_H
#define BROWSER_BLOCKFORMATTINGCONTEXT_H
#include "FormattingContext.h"

struct LayoutResult {
    LayoutBox box;

    double escapedMarginBottom = 0;
};


class LayoutGenerator;

class BlockFormattingContext : public FormattingContext {
public:
    explicit BlockFormattingContext(LayoutGenerator& lr) : lr_(lr) {}

    int Layout(Node& node, LayoutBox& parent, int contentX, int contentY, int contentWidth, int contentHeight) override;
    [[nodiscard]] int GetLastChildMarginBottom() const override { return lastChildMarginBottom_; }
private:
    size_t LayoutInlineRun(std::vector<std::unique_ptr<Node>>& kids, size_t start,
                           LayoutBox& parent, int contentX, int contentY,
                           int contentWidth, int contentHeight, TextAlign align, int& cursorY) const;
    int lastChildMarginBottom_ = 0;
    LayoutGenerator& lr_;
};


#endif //BROWSER_BLOCKFORMATTINGCONTEXT_H
