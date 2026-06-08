//
// Created by tkdtu on 5/31/2026.
//

#ifndef BROWSER_LAYOUTRENDERER_H
#define BROWSER_LAYOUTRENDERER_H
#include "LayoutHelper.h"
#include "Render/Renderer.h"
inline static Color EncodePickID(uint32_t id) {
    return { uint8_t(id >> 16), uint8_t(id >> 8), uint8_t(id), 255 };
}
inline static uint32_t DecodePickID(Color c) {
    return (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | c.b;
}
struct TextHitResult {
    LayoutBox* box = nullptr;
    int offset = 0;
    bool valid = false;
};

class LayoutRenderer {
public:
    explicit LayoutRenderer(RendererSurface& renderer);

    void resize(int x, int y);
    void RenderRoot(LayoutBox& root);
    void UpdateDom(Node *dom);
    LayoutBox* HitTest(int x, int y);
    int GetCharacterOffsetAtX(LayoutBox& run, int mouseX);
    TextHitResult HitTestTextPosition(
        LayoutBox& root,
        int mouseX,
        int mouseY);
private:
    void Render(LayoutBox& box);

    void RenderBox(LayoutBox& box);

    static bool IsImageBox(LayoutBox& box);

    void RenderImage(LayoutBox& box) const;

    void RenderTextRun(LayoutBox& box);
    void RenderBlock(LayoutBox& box);

    void RenderSingleBorderEdge(const BorderSide& edge, int start, int end, int fixedCoord, bool isHorizontal, float fontSize);
    void RenderLineSelection(LayoutBox& line);

    void RenderLine(LayoutBox& box, int textHeight);
    void RenderDecoration(
        TextDecorationStyle style,
        Color color, int startX, int y, int thickness, int endX);


private:
    Node *Dom{};
    Node *Body{};
    RendererSurface& renderer;
    RenderTargetID   pickTarget  = 0;     // 0 = disabled
    bool             isPickPass  = false;

    uint32_t                          nextPickID = 1;
    std::unordered_map<uint32_t, LayoutBox*> pickMap;

    void SetPickTarget(RenderTargetID t) { pickTarget = t; }




    Color AllocPickColor(LayoutBox* node);
};


#endif //BROWSER_LAYOUTRENDERER_H
