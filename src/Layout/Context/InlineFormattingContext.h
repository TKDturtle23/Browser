//
// Created by tkdtu on 5/31/2026.
//

#ifndef BROWSER_INLINEFORMATTINGCONTEXT_H
#define BROWSER_INLINEFORMATTINGCONTEXT_H
#include "FormattingContext.h"
#include "Layout/WordCollector.h"


class LayoutGenerator;

class InlineFormattingContext : public FormattingContext {
public:
    InlineFormattingContext(LayoutGenerator& lr, TextAlign align)
        : lr_(lr), align_(align) {}

    int LayoutRoots(std::vector<Node*>& roots,
                    LayoutBox& parent,
                    int startX, int startY, int containerWidth) const;

    int Layout(Node&, LayoutBox&, int, int contentY, int) override {
        return contentY;
    }

private:
    static void FinalizeLineMetrics(LayoutBox& line, LayoutGenerator& lr);

    LayoutGenerator& lr_;
    TextAlign       align_;
};




#endif //BROWSER_INLINEFORMATTINGCONTEXT_H
