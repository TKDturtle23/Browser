//
// Created by tkdtu on 6/10/2026.
//

#ifndef BROWSER_FLEXFORMATTINGCONTEXT_H
#define BROWSER_FLEXFORMATTINGCONTEXT_H
#include "FormattingContext.h"

class LayoutGenerator;
struct FlexItem {
    Node*     node{};
    LayoutBox box;

    // Main axis
    int   baseSize{};        // resolved flex-basis
    int   hypothetical{};    // clamped to min/max
    int   finalMainSize{};   // after grow/shrink
    float growFactor{};
    float shrinkFactor{};
    int   marginMainStart{}; // resolved auto margin before item
    int   marginMainEnd{};   // resolved auto margin after item

    // Cross axis
    int  crossSize{};            // natural cross-axis size after layout
    int  specifiedCrossSize{};   // explicit width/height on cross axis
    bool hasCrossSize{};         // whether specifiedCrossSize is valid
};
class FlexFormattingContext : public FormattingContext {
public:
    explicit FlexFormattingContext(LayoutGenerator& lr) : lr_(lr) {}

    int Layout(Node& node, LayoutBox& parent, int contentX, int contentY, int contentWidth, int contentHeight) override;
    int GetLastChildMarginBottom() const override;

private:
    int lastChildMarginBottom_ = 0;
    LayoutGenerator& lr_;
};


#endif //BROWSER_FLEXFORMATTINGCONTEXT_H
