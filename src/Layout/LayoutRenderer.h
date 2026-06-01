//
// Created by tkdtu on 5/31/2026.
//

#ifndef BROWSER_LAYOUTRENDERER_H
#define BROWSER_LAYOUTRENDERER_H
#include "LayoutHelper.h"
#include "Render/Renderer.h"



class LayoutRenderer {
public:
    explicit LayoutRenderer(Renderer& renderer, Font& FallbackFont);

    Font &ResolveFont(const Style &s);

    FontMetrics PrepareFontContext(const Style &s, int forcedSize, Font *&outFont);



    void RenderRoot(const LayoutBox& root);
    void UpdateDom(Node *dom);
    void AddFont(std::string name, FontGroup& group);
private:
    void Render(const LayoutBox &box);

    void RenderBox(const LayoutBox& box);

    static bool IsImageBox(const LayoutBox &box);

    void RenderImage(const LayoutBox &box) const;

    void RenderTextRun(const LayoutBox& box);
    void RenderBlock(const LayoutBox& box);

    void RenderSingleBorderEdge(const Border_side &edge, int start, int end, int fixedCoord, bool isHorizontal);

    void RenderLine(const LayoutBox &box, int Text_Height);
    void RenderDecoration(
        TextDecorationStyle style,
        Color color, int startX, int y, int thickness, int endX);


private:
    Node *Dom{};
    Node *Body{};
    Renderer& renderer;
    std::unordered_map<std::string, std::reference_wrapper<FontGroup>> Fonts;
    Font &criticalFallbackFont;
};


#endif //BROWSER_LAYOUTRENDERER_H
