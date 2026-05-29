//
// Created by tkdtu on 5/27/2026.
//

#ifndef BROWSER_LAYOUTRENDERER_H
#define BROWSER_LAYOUTRENDERER_H
#include "../Render/Renderer.h"
#include "../Parser.h"
#include "../Text/Font.h"

enum class BoxKind {
    Block,
    Line,
    TextRun,
};

struct LayoutBox {
    BoxKind kind = BoxKind::Block;

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int fontSize = 0; // for TextRun boxes

    int lineAscent = 0;
    int lineDescent = 0;

    const Node* node = nullptr;

    // Only used when kind == TextRun.
    std::string text;

    std::vector<LayoutBox> children;
};

// ---------------------------------------------------------------------------
//  FormattingContext — one subclass per layout model
//
//  To add a new model (flex, grid, …) subclass this and implement Layout().
//  LayoutBlock() picks the right context based on the node's computed style.
// ---------------------------------------------------------------------------
class FormattingContext {
public:
    virtual ~FormattingContext() = default;

    // Lay out all children of `node` starting at (contentX, contentY) inside
    // a content area of `contentWidth` pixels.  Appends child boxes into
    // `parent` and returns the Y coordinate just below the last child.
    virtual int Layout(const Node& node,
                       LayoutBox& parent,
                       int contentX,
                       int contentY,
                       int contentWidth) = 0;
};

// ---------------------------------------------------------------------------
//  LayoutRenderer
// ---------------------------------------------------------------------------
class LayoutRenderer {
public:
    FontMetrics PrepareFontContext(const Style &s, int forcedSize, Font *&outFont);

    explicit LayoutRenderer(Renderer& renderer);

    // Full layout + render pass driven by the DOM root.
    void Update(const Node& dom);
    void Render(const LayoutBox &box);
    void Render();
    // Public so FormattingContext subclasses can reuse them.
    Font& ResolveFont(const Style& s);

    Font BaseFont;
    Font BaseItalicFont;
    Font BaseBoldFont;
    Font BaseBoldItalicFont;
    // Lay out a single block-level node.  Delegates child layout to the
    // appropriate FormattingContext.
    LayoutBox LayoutBlock(const Node& node,
                          int containerX,
                          int containerY,
                          int containerWidth);
private:
    Renderer& renderer;
    LayoutBox root;

    // --- layout helpers ---



    // --- render helpers ---
    void RenderBox(const LayoutBox& box);

    void RenderImage(const LayoutBox &box) const;

    void RenderTextRun(const LayoutBox& box);
    void RenderBlock(const LayoutBox& box);

    void RenderSingleBorderEdge(const Border_side &edge, int start, int end, int fixedCoord, bool isHorizontal);

    void RenderLine(const LayoutBox &box, int Text_Height);
    void RenderDecoration(
        TextDecorationStyle style,
        Color color, int startX, int y, int thickness, int endX);

    // Searches the layout tree for the first node with a background color and
    // uses it as the window clear color.
    Color FindWindowBackground() const;
};


#endif //BROWSER_LAYOUTRENDERER_H
