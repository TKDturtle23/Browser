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
#define ResolveFromWindow(length, baseWidth, inheritedFontSize) ResolveLength(length, baseWidth, renderer.GetWidth(), renderer.GetHeight(), inheritedFontSize)



LayoutGenerator::LayoutGenerator(RendererSurface& renderer)
    : renderer(renderer)
{}



BoxEdges LayoutGenerator::ResolvePadding(const Style& s, int containerWidth, int resolved_font_size) const {

    return {
        ResolveFromWindow(s.padding_top,    containerWidth, resolved_font_size),
        ResolveFromWindow(s.padding_right,   containerWidth, resolved_font_size),
        ResolveFromWindow(s.padding_bottom, containerWidth, resolved_font_size),
        ResolveFromWindow(s.padding_left,   containerWidth, resolved_font_size)
    };
}

BoxEdges LayoutGenerator::ResolveBorders(const Style& s, float FontSize) const {
    return {
        GetVisibleBorderWidth(s.borderTop,    renderer.GetWidth(), renderer.GetHeight(), FontSize),
        GetVisibleBorderWidth(s.borderRight,   renderer.GetWidth(), renderer.GetHeight(), FontSize),
        GetVisibleBorderWidth(s.borderBottom,  renderer.GetWidth(), renderer.GetHeight(), FontSize),
        GetVisibleBorderWidth(s.borderLeft,    renderer.GetWidth(), renderer.GetHeight(), FontSize)
    };
}

BoxEdges LayoutGenerator::ResolveMargins(const Style& s, int containerWidth, int resolved_font_size) const {
    return {
        (s.margin_top.unit    == LengthUnit::Auto) ? 0 : ResolveFromWindow(s.margin_top,    containerWidth, resolved_font_size),
        (s.margin_right.unit  == LengthUnit::Auto) ? 0 : ResolveFromWindow(s.margin_right,  containerWidth, resolved_font_size),
        (s.margin_bottom.unit == LengthUnit::Auto) ? 0 : ResolveFromWindow(s.margin_bottom, containerWidth, resolved_font_size),
        (s.margin_left.unit   == LengthUnit::Auto) ? 0 : ResolveFromWindow(s.margin_left,   containerWidth, resolved_font_size)
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

BlockResult LayoutGenerator::LayoutBlock(Node& node, int containerX, int containerY, int containerWidth, int containerHeight) {
    const Style& s = node.computedStyle;
    int InheritedFontSize = ResolveFontSizeInherit(&node, renderer.GetWidth(), renderer.GetHeight());
    // 1. Resolve Box Model Geometry
    BoxEdges padding = ResolvePadding(s, containerWidth, InheritedFontSize);
    BoxEdges border  = ResolveBorders(s, InheritedFontSize);
    BoxEdges margin  = ResolveMargins(s, containerWidth, InheritedFontSize);

    // 2. Resolve Core Width
    bool hasExplicitWidth = (s.width.unit != LengthUnit::Auto);
    LayoutBox box;
    box.kind = BoxKind::Block;
    box.node = &node;

    if (hasExplicitWidth) {
        int computedContent = ResolveFromWindow(s.width, containerWidth, InheritedFontSize);
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

    int contentX      = box.x + border.left + padding.left;
    int contentY      = box.y + border.top  + padding.top;
    int contentWidth  = std::max(0, static_cast<int>(box.width - padding.Horizontal() - border.Horizontal()));
    int contentHeight = std::max(0, static_cast<int>(containerHeight - padding.Vertical() - border.Vertical()));

    // 4. Layout Children via BFC
    auto ctx = std::make_unique<BlockFormattingContext>(*this);
    int endY = ctx->Layout(node, box, contentX, contentY, contentWidth, contentHeight);

    // Retrieve the last child's trailing margin.
    // If this box has no bottom border or padding, the margin collapses upward
    // into the parent BFC (CSS §8.3.1). We signal this via escapedMarginBottom
    // so the parent BFC can fold it into its own prevMarginBottom and perform
    // the three-way collapse with the next sibling's margin-top.
    // If there IS a bottom edge, the margin is contained — add it to endY so
    // it contributes to this box's height, and report zero escaped margin.
    double trailingMargin   = ctx->GetLastChildMarginBottom();
    bool hasBottomEdge   = (padding.bottom > 0 || border.bottom > 0);
    double escapedMargin    = 0;

    if (hasBottomEdge) {
        endY += trailingMargin;   // last child's margin is contained
    } else {
        // Last child's margin collapses through to us
        escapedMargin = std::max(escapedMargin, trailingMargin);
    }

    // Always: our own margin_bottom escapes to the parent BFC
    escapedMargin = std::max(escapedMargin, margin.bottom);
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
        int computedContent = ResolveFromWindow(s.height, containerHeight, InheritedFontSize);
        box.height = (s.boxSizing == BoxSizing::ContentBox)
                     ? computedContent + padding.Vertical() + border.Vertical()
                     : computedContent;
        // Explicit height contains the margin entirely — nothing escapes.
        escapedMargin = 0;
    } else {
        box.height = (endY - contentY) + padding.Vertical() + border.Vertical();
    }

    // Clamp min/max height
    if (s.max_height.unit != LengthUnit::Auto) box.height = std::min(box.height, (int)ResolveFromWindow(s.max_height, 0, InheritedFontSize));
    if (s.min_height.unit != LengthUnit::Auto) box.height = std::max(box.height, (int)ResolveFromWindow(s.min_height, 0, InheritedFontSize));

    // 7. Write Back Debug Data
    auto& rd = box.node->renderData;
    rd.box = { (float)box.x, (float)box.y, (float)box.width, (float)box.height };

    rd.resolved_margin_top    = margin.top;    rd.resolved_margin_right  = margin.right;
    rd.resolved_margin_bottom = margin.bottom; rd.resolved_margin_left   = margin.left;

    rd.resolved_padding_top    = padding.top;    rd.resolved_padding_right  = padding.right;
    rd.resolved_padding_bottom = padding.bottom; rd.resolved_padding_left   = padding.left;

    return { std::move(box), escapedMargin };
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

    int bodyMarginTop = ResolveLength(body->computedStyle.margin_top, renderer.GetWidth(), renderer.GetWidth(),
        renderer.GetHeight(), ResolveFontSize(body->computedStyle.font_size, renderer.GetWidth(), renderer.GetHeight(), 16));
    // Update() is the root of the layout tree — nothing above it to collapse
    // into, so we discard escapedMarginBottom here intentionally.
    BlockResult result = LayoutBlock(*body, 0, bodyMarginTop, renderer.GetWidth(), renderer.GetHeight());
    root = std::move(result.box);
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