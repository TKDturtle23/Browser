//
// Created by tkdtu on 5/31/2026.
//

#ifndef BROWSER_FORMATTINGCONTEXT_H
#define BROWSER_FORMATTINGCONTEXT_H
#include "Layout/LayoutHelper.h"


class FormattingContext {
public:
    virtual ~FormattingContext() = default;

    // Lay out all children of `node` starting at (contentX, contentY) inside
    // a content area of `contentWidth` pixels.  Appends child boxes into
    // `parent` and returns the Y coordinate just below the last child.
    virtual int Layout(Node& node,
                       LayoutBox& parent,
                       int contentX,
                       int contentY,
                       int contentWidth) = 0;
};


#endif //BROWSER_FORMATTINGCONTEXT_H
