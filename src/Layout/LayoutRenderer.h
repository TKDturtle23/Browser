//
// Created by tkdtu on 5/31/2026.
//

#ifndef BROWSER_LAYOUTRENDERER_H
#define BROWSER_LAYOUTRENDERER_H
#include "LayoutHelper.h"
#include "Render/Renderer.h"



class LayoutRenderer {
public:
    explicit LayoutRenderer(RendererSurface& renderer);


    void RenderRoot(const LayoutBox& root);
    void UpdateDom(Node *dom);

private:
    void Render(const LayoutBox &box);

    void RenderBox(const LayoutBox& box);

    static bool IsImageBox(const LayoutBox &box);

    void RenderImage(const LayoutBox &box) const;

    void RenderTextRun(const LayoutBox& box);
    void RenderBlock(const LayoutBox& box);

    void RenderSingleBorderEdge(const BorderSide &edge, int start, int end, int fixedCoord, bool isHorizontal);

    void RenderLine(const LayoutBox &box, int Text_Height);
    void RenderDecoration(
        TextDecorationStyle style,
        Color color, int startX, int y, int thickness, int endX);


private:
    Node *Dom{};
    Node *Body{};
    RendererSurface& renderer;
};


#endif //BROWSER_LAYOUTRENDERER_H
