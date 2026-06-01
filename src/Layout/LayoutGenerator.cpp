// LayoutRenderer.cpp

#include "LayoutGenerator.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <functional>
#include <iostream>
#include <unordered_set>
#include "LayoutHelper.h"
#include "WordCollector.h"
#include "Context/BlockFormattingContext.h"
#define ResolveFromWindow(length, baseWidth) ResolveLength(length, baseWidth, renderer.GetWidth(), renderer.GetHeight())



LayoutGenerator::LayoutGenerator(Renderer& renderer)
    : renderer(renderer)
    , BaseFont           ("arial/ARIAL.TTF",   16)
    , BaseItalicFont     ("arial/ARIALI.TTF",  16)
    , BaseBoldFont       ("arial/ARIALBD.TTF", 16)
    , BaseBoldItalicFont ("arial/ARIALBI.TTF", 16)
{}



BoxEdges LayoutGenerator::ResolvePadding(const Style& s, int containerWidth) const {
    return {
        ResolveFromWindow(s.padding_top,    containerWidth),
        ResolveFromWindow(s.padding_right,   containerWidth),
        ResolveFromWindow(s.padding_bottom, containerWidth),
        ResolveFromWindow(s.padding_left,   containerWidth)
    };
}

BoxEdges LayoutGenerator::ResolveBorders(const Style& s) const {
    return {
        GetVisibleBorderWidth(s.BorderTop,    renderer.GetWidth(), renderer.GetHeight()),
        GetVisibleBorderWidth(s.BorderRight,   renderer.GetWidth(), renderer.GetHeight()),
        GetVisibleBorderWidth(s.BorderBottom,  renderer.GetWidth(), renderer.GetHeight()),
        GetVisibleBorderWidth(s.BorderLeft,    renderer.GetWidth(), renderer.GetHeight())
    };
}

BoxEdges LayoutGenerator::ResolveMargins(const Style& s, int containerWidth) const {
    return {
        (s.margin_top.unit    == LengthUnit::Auto) ? 0 : ResolveFromWindow(s.margin_top,    containerWidth),
        (s.margin_right.unit  == LengthUnit::Auto) ? 0 : ResolveFromWindow(s.margin_right,  containerWidth),
        (s.margin_bottom.unit == LengthUnit::Auto) ? 0 : ResolveFromWindow(s.margin_bottom, containerWidth),
        (s.margin_left.unit   == LengthUnit::Auto) ? 0 : ResolveFromWindow(s.margin_left,   containerWidth)
    };
}

void LayoutGenerator::ApplyMarginCentering(const Style& s, BoxEdges& margin, int containerWidth, int boxWidth) const {
    int remaining = containerWidth - boxWidth;
    if (remaining <= 0) return;

    bool autoLeft  = (s.margin_left.unit  == LengthUnit::Auto);
    bool autoRight = (s.margin_right.unit == LengthUnit::Auto);

    if (autoLeft && autoRight) {
        margin.left  = remaining / 2;
        margin.right = remaining - margin.left;
    } else if (autoLeft) {
        margin.left  = remaining - margin.right;
    } else if (autoRight) {
        margin.right = remaining - margin.left;
    }
}
LayoutBox LayoutGenerator::LayoutBlock(Node& node, int containerX, int containerY, int containerWidth) {
    const Style& s = node.computedStyle;

    // 1. Resolve Box Model Geometry
    BoxEdges padding = ResolvePadding(s, containerWidth);
    BoxEdges border  = ResolveBorders(s);
    BoxEdges margin  = ResolveMargins(s, containerWidth);

    // 2. Resolve Core Width
    bool hasExplicitWidth = (s.width.unit != LengthUnit::Auto);
    LayoutBox box;
    box.kind = BoxKind::Block;
    box.node = &node;

    if (hasExplicitWidth) {
        int computedContent = ResolveFromWindow(s.width, containerWidth);
        box.width = (s.boxSizing == BoxSizing::ContentBox)
                    ? computedContent + padding.Horizontal() + border.Horizontal()
                    : computedContent;
    } else {
        box.width = containerWidth;
    }

    // 3. Position Box & Form Content Bounds
    ApplyMarginCentering(s, margin, containerWidth, box.width);
    box.x = containerX + margin.left;
    box.y = containerY;

    int contentX     = box.x + border.left + padding.left;
    int contentY     = box.y + border.top  + padding.top;
    int contentWidth = std::max(0, box.width - padding.Horizontal() - border.Horizontal());

    // 4. Layout Children via BFC
    auto ctx = std::make_unique<BlockFormattingContext>(*this);
    int endY = ctx->Layout(node, box, contentX, contentY, contentWidth);

    // 5. Shrink-to-Fit Pass
    bool shrinkToFit = (node.tag == "span" || node.tag == "button");
    if (!hasExplicitWidth && shrinkToFit && !box.children.empty()) {
        int maxRight = contentX;
        for (const auto& cb : box.children) {
            maxRight = std::max(maxRight, cb.x + cb.width);
        }
        box.width = (maxRight - contentX) + padding.Horizontal() + border.Horizontal();

        // Re-center if margins were auto
        int finalSpace = containerWidth - box.width;
        if (finalSpace > 0 && s.margin_left.unit == LengthUnit::Auto && s.margin_right.unit == LengthUnit::Auto) {
            box.x = containerX + finalSpace / 2;
        }
    }

    // 6. Resolve Height
    if (s.height.unit != LengthUnit::Auto) {
        int computedContent = ResolveFromWindow(s.height, 0);
        box.height = (s.boxSizing == BoxSizing::ContentBox)
                     ? computedContent + padding.Vertical() + border.Vertical()
                     : computedContent;
    } else {
        box.height = (endY - contentY) + padding.Vertical() + border.Vertical();
    }

    // Clamp min/max height
    if (s.max_height.unit != LengthUnit::Auto) box.height = std::min(box.height, ResolveFromWindow(s.max_height, 0));
    if (s.min_height.unit != LengthUnit::Auto) box.height = std::max(box.height, ResolveFromWindow(s.min_height, 0));

    // 7. Write Back Debug Data
    auto& rd = box.node->renderData;
    rd.box = { box.x, box.y, box.width, box.height };

    rd.margin_top    = margin.top;    rd.margin_right  = margin.right;
    rd.margin_bottom = margin.bottom; rd.margin_left   = margin.left;

    rd.padding_top    = padding.top;    rd.padding_right  = padding.right;
    rd.padding_bottom = padding.bottom; rd.padding_left   = padding.left;

    return box;
}
void LayoutGenerator::Update(Node& dom) {
    body = nullptr;

    std::function<Node*(Node&)> findBody = [&](Node& n) -> Node* {
        for (const auto& child : n.children) {
            if (child->tag == "body") return child.get();
            if (!IsLayoutIgnored(*child))
                if (auto* r = findBody(*child)) return r;
        }
        return nullptr;
    };

    body = findBody(dom);
    assert(body && "DOM must contain a <body> element");

    int bodyMarginTop = ResolveLength(body->computedStyle.margin_top, renderer.GetWidth(), renderer.GetWidth(), renderer.GetHeight());
    root = LayoutBlock(*body, 0, bodyMarginTop, renderer.GetWidth());
}


Color LayoutGenerator::FindWindowBackground() const {
    std::function<Color(const LayoutBox&)> find = [&](const LayoutBox& b) -> Color {
        if (b.node && b.node->computedStyle.hasBackground)
            return b.node->computedStyle.backgroundColor;
        for (const auto& c : b.children) {
            Color r = find(c);
            if (r.a != 0) return r;
        }
        return Color(0, 0, 0, 0);
    };
    Color bg = find(root);
    return (bg.a != 0) ? bg : Color(255, 255, 255);
}
