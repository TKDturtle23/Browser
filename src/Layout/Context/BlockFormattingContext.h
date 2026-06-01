//
// Created by tkdtu on 5/31/2026.
//

#ifndef BROWSER_BLOCKFORMATTINGCONTEXT_H
#define BROWSER_BLOCKFORMATTINGCONTEXT_H
#include "FormattingContext.h"



class LayoutGenerator;

class BlockFormattingContext : public FormattingContext {
public:
    explicit BlockFormattingContext(LayoutGenerator& lr) : lr_(lr) {}

    int Layout(Node& node, LayoutBox& parent, int contentX, int contentY, int contentWidth) override;

private:
    size_t LayoutInlineRun(std::vector<std::unique_ptr<Node>>& kids, size_t start,
                           LayoutBox& parent, int contentX, int contentY,
                           int contentWidth, TextAlign align, int& cursorY) const;

    LayoutGenerator& lr_;
};


#endif //BROWSER_BLOCKFORMATTINGCONTEXT_H
