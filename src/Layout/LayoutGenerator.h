//
// Created by tkdtu on 5/27/2026.
//

#ifndef BROWSER_LayoutGenerator_H
#define BROWSER_LayoutGenerator_H
#include "../Render/Renderer.h"
#include "../Parser.h"
#include "../Text/Font.h"
#include "LayoutHelper.h"

// ---------------------------------------------------------------------------
//  LayoutRenderer
// ---------------------------------------------------------------------------
class LayoutGenerator {
public:

    explicit LayoutGenerator(RendererSurface& renderer);

    // Full layout + render pass driven by the DOM root.
    void Update(Node& dom);



    BoxEdges ResolvePadding(const Style &s, int containerWidth) const;

    BoxEdges ResolveBorders(const Style &s) const;

    BoxEdges ResolveMargins(const Style &s, int containerWidth) const;

    void ApplyMarginCentering(const Style &s, BoxEdges &margin, int containerWidth, int boxWidth) const;


    // Lay out a single block-level node.  Delegates child layout to the
    // appropriate FormattingContext.
    LayoutBox LayoutBlock(Node& node,
                          int containerX,
                          int containerY,
                          int containerWidth, int containerHeight);

    int GetWidth() const { return renderer.GetWidth(); }
    int GetHeight() const { return renderer.GetHeight(); }

    LayoutBox& GetRoot() { return root; }
private:
    RendererSurface& renderer;
    LayoutBox root;
    Node *body = nullptr;



    // Searches the layout tree for the first node with a background color and
    // uses it as the window clear color.
    Color FindWindowBackground() const;
};


#endif //BROWSER_LAYOUTRENDERER_H
