#include "Color.h"
#include "DebugOverlayRenderer.h"

void DebugOverlayRenderer::DrawBoxModel(
    RendererSurface& renderer,
    const Node* node,
    int topBarHeight)
{
    if (!node) return;

    const int bx = node->renderData.box.x;
    const int by = node->renderData.box.y + topBarHeight;
    const int bw = node->renderData.box.width;
    const int bh = node->renderData.box.height;

    const int pTop    = node->renderData.resolved_padding_top;
    const int pBottom = node->renderData.resolved_padding_bottom;
    const int pLeft   = node->renderData.resolved_padding_left;
    const int pRight  = node->renderData.resolved_padding_right;

    const int mTop    = node->renderData.resolved_margin_top;
    const int mBottom = node->renderData.resolved_margin_bottom;
    const int mLeft   = node->renderData.resolved_margin_left;
    const int mRight  = node->renderData.resolved_margin_right;

    // Margin box
    const int mx = bx - mLeft;
    const int my = by - mTop;
    const int mw = bw + mLeft + mRight;
    const int mh = bh + mTop + mBottom;

    // Border/Padding box
    const int px = bx;
    const int py = by;
    const int pw = bw;
    const int ph = bh;

    // Content box
    const int cx = bx + pLeft;
    const int cy = by + pTop;
    const int cw = std::max(0, bw - pLeft - pRight);
    const int ch = std::max(0, bh - pTop - pBottom);

    // Filled regions
    renderer.FillRect(mx, my, mw, mh, Color(0, 255, 255, 64));   // margin
    renderer.FillRect(px, py, pw, ph, Color(255, 0, 255, 64));   // padding
    renderer.FillRect(cx, cy, cw, ch, Color(0, 0, 255, 64));     // content

    // Outlines
    renderer.DrawRect(mx, my, mw, mh, Color(0, 255, 255, 200));
    renderer.DrawRect(px, py, pw, ph, Color(255, 0, 255, 200));
    renderer.DrawRect(cx, cy, cw, ch, Color(255, 255, 255, 220));
}