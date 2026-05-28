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


    const Node* node = nullptr;

    // Only used when kind == TextRun.
    std::string text;

    std::vector<LayoutBox> children;
};

class LayoutRenderer {
public:
    LayoutRenderer(Renderer& renderer, Font& font);

    void Update(const Node& dom);
    void Render();

private:
    Renderer& renderer;
    Font& font;

    LayoutBox root;

    LayoutBox LayoutBlock(const Node& node, int containerX, int containerY, int containerWidth);

    // Lays a contiguous run of inline-level siblings into line boxes.
    // Returns the line boxes; advances *outNextY to the y just below the last line.
    std::vector<LayoutBox> LayoutInline(
        const std::vector<const Node*>& inlineRoots,
        int startX,
        int startY,
        int containerWidth,
        TextAlign textAlign,
        int* outNextY);

    void RenderBox(const LayoutBox& box);
};


#endif //BROWSER_LAYOUTRENDERER_H
