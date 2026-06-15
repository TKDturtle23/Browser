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
#include "Context/FlexFormattingContext.h"
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

void LayoutGenerator::ApplyMarginCentering(const Style& s, BoxEdges& margin, int containerWidth, int boxWidth) {
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
LayoutResult LayoutGenerator::LayoutNode(
    Node& node,
    int containerX,
    int containerY,
    int containerWidth,
    int containerHeight)
{
    const Style& s = node.computedStyle;

    switch (s.display)
    {
        case DisplayType::Flex:
            return LayoutFlex(
                node,
                containerX,
                containerY,
                containerWidth,
                containerHeight);

        case DisplayType::Grid:
            std::cout << "Grid not yet implemented, defaulting to block" << std::endl;
        case DisplayType::Block:
        case DisplayType::Inline:
        default:
            return LayoutBlock(
                node,
                containerX,
                containerY,
                containerWidth,
                containerHeight);
    }
}
LayoutResult LayoutGenerator::LayoutFlex(
    Node& node,
    int containerX,
    int containerY,
    int containerWidth,
    int containerHeight)
{
    const Style& s = node.computedStyle;

    int inheritedFontSize =
        ResolveFontSizeInherit(&node, renderer.GetWidth(), renderer.GetHeight());

    // ─────────────────────────────────────────────────────────────
    // Resolve box model
    // ─────────────────────────────────────────────────────────────

    BoxEdges padding = ResolvePadding(s, containerWidth, inheritedFontSize);
    BoxEdges border  = ResolveBorders(s, inheritedFontSize);
    BoxEdges margin  = ResolveMargins(s, containerWidth, inheritedFontSize);

    LayoutBox box;
    box.kind = BoxKind::Block;
    box.node = &node;

    // Width
    if (s.width.unit != LengthUnit::Auto) {
        int computed =
            ResolveFromWindow(s.width, containerWidth, inheritedFontSize);

        box.width =
            (s.boxSizing == BoxSizing::ContentBox)
                ? computed + padding.Horizontal() + border.Horizontal()
                : computed;
    } else {
        box.width = containerWidth;
    }

    ApplyMarginCentering(s, margin, containerWidth, box.width);

    box.x = containerX + margin.left;
    box.y = containerY;

    int contentX =
        box.x + border.left + padding.left;

    int contentY =
        box.y + border.top + padding.top;

    int contentWidth =
        std::max(0.0, box.width - padding.Horizontal() - border.Horizontal());

    int contentHeight =
        std::max(0.0, containerHeight - padding.Vertical() - border.Vertical());

    // ─────────────────────────────────────────────────────────────
    // Flex formatting context
    // ─────────────────────────────────────────────────────────────

    FlexFormattingContext ffc(*this);

    int endMain =
        ffc.Layout(
            node,
            box,
            contentX,
            contentY,
            contentWidth,
            contentHeight);

    // ─────────────────────────────────────────────────────────────
    // Resolve height
    //
    // endMain is an absolute Y (contentY + accumulated cursor).
    // For column: (endMain - contentY) is the content block height.
    // For row:    height is driven by the tallest child (maxCross),
    //             read back from the children already pushed into box.
    // ─────────────────────────────────────────────────────────────

    bool isRow = (s.flex.direction == FlexDirection::Row);
    std::cout << "LayoutFlex: containerHeight=" << containerHeight
              << " contentHeight=" << contentHeight
              << " availableCross=" << (isRow ? contentHeight : contentWidth)
              << " node=" << node.tag << std::endl;
    if (s.height.unit != LengthUnit::Auto)
    {
        int computed =
            ResolveFromWindow(s.height, containerHeight, inheritedFontSize);

        box.height =
            (s.boxSizing == BoxSizing::ContentBox)
                ? computed + padding.Vertical() + border.Vertical()
                : computed;
    }
    else if (isRow)
    {
        // Row flex: height = tallest child bottom edge, measured from box.y.
        int maxBottom = contentY;

        for (const auto& child : box.children)
            maxBottom = std::max(maxBottom, child.y + child.height);

        box.height =
            (maxBottom - box.y)
            + border.bottom
            + padding.bottom;
    }
    else
    {
        // Column flex: endMain is absolute, so subtract contentY to get
        // the content block height, then add padding + border.
        // Note: do NOT also add padding.top / border.top — those are
        // already encoded in the offset between box.y and contentY.
        int contentBlockHeight = endMain - contentY;

        box.height =
            contentBlockHeight
            + padding.Vertical()
            + border.Vertical();
    }

    // ─────────────────────────────────────────────────────────────
    // Min/max clamps
    // ─────────────────────────────────────────────────────────────

    if (s.max_height.unit != LengthUnit::Auto)
    {
        box.height =
            std::min(
                box.height,
                (int)ResolveFromWindow(s.max_height, 0, inheritedFontSize));
    }

    if (s.min_height.unit != LengthUnit::Auto)
    {
        box.height =
            std::max(
                box.height,
                (int)ResolveFromWindow(s.min_height, 0, inheritedFontSize));
    }

    // ─────────────────────────────────────────────────────────────
    // Render debug data
    // ─────────────────────────────────────────────────────────────

    auto& rd = box.node->renderData;

    rd.box = {
        (float)box.x,
        (float)box.y,
        (float)box.width,
        (float)box.height
    };

    rd.resolved_margin_top    = margin.top;
    rd.resolved_margin_right  = margin.right;
    rd.resolved_margin_bottom = margin.bottom;
    rd.resolved_margin_left   = margin.left;

    rd.resolved_padding_top    = padding.top;
    rd.resolved_padding_right  = padding.right;
    rd.resolved_padding_bottom = padding.bottom;
    rd.resolved_padding_left   = padding.left;

    // Flex containers establish a new formatting context.
    // Margins do not collapse through flex containers.
    return {
        std::move(box),
        0
    };
}
LayoutResult LayoutGenerator::LayoutBlock(Node& node, int containerX, int containerY, int containerWidth, int containerHeight) {
    const Style& s = node.computedStyle;
    int inheritedFontSize = ResolveFontSizeInherit(&node, renderer.GetWidth(), renderer.GetHeight());

    // 1. Resolve Box Model Geometry
    BoxEdges padding = ResolvePadding(s, containerWidth, inheritedFontSize);
    BoxEdges border  = ResolveBorders(s, inheritedFontSize);
    BoxEdges margin  = ResolveMargins(s, containerWidth, inheritedFontSize);

    bool hasExplicitWidth = (s.width.unit != LengthUnit::Auto);
    bool shrinkToFit = (node.tag == "span" || node.tag == "button" ||
                        s.display == DisplayType::Inline || s.display == DisplayType::InlineBlock);

    LayoutBox box;
    box.kind = BoxKind::Block;
    box.node = &node;

    // STEP 1: Core Width Resolution
    if (hasExplicitWidth) {
        int computedContent = ResolveFromWindow(s.width, containerWidth, inheritedFontSize);
        box.width = (s.boxSizing == BoxSizing::ContentBox)
            ? computedContent + padding.Horizontal() + border.Horizontal()
            : computedContent;
    } else if (shrinkToFit) {
        box.width = 0; // Handled dynamically post-BFC pass or via intrinsic pass
    } else {
        box.width = containerWidth;
    }

    // 2. Center/Position Box (Only if width is already known)
    if (!shrinkToFit || hasExplicitWidth) {
        ApplyMarginCentering(s, margin, containerWidth, box.width);
        box.x = containerX + margin.left;
    } else {
        // Temporary assignment for content positioning; final box.x computed in Step 5
        box.x = containerX + (s.margin_left.unit == LengthUnit::Auto ? 0 : margin.left);
    }
    box.y = containerY;
    box.y = containerY;

    int contentX = box.x + border.left + padding.left;
    int contentY = box.y + border.top + padding.top;

    int contentWidth = hasExplicitWidth
        ? std::max((double)0, (s.boxSizing == BoxSizing::ContentBox)
            ? ResolveFromWindow(s.width, containerWidth, inheritedFontSize)
            : ResolveFromWindow(s.width, containerWidth, inheritedFontSize) - padding.Horizontal() - border.Horizontal())
        : containerWidth;

    // Note: Adjust containerHeight base depending on parent's BoxSizing strategy
    int contentHeight = std::max((double)0, containerHeight - padding.Vertical() - border.Vertical());

    // 3. Layout Children via BFC
    auto ctx = std::make_unique<BlockFormattingContext>(*this);
    int endY = ctx->Layout(node, box, contentX, contentY, contentWidth, contentHeight);

    // 4. Margin Collapsing Logic
    int trailingMargin = ctx->GetLastChildMarginBottom();
    bool hasBottomEdge = (padding.bottom > 0 || border.bottom > 0);
    int escapedMargin  = 0;

    if (hasBottomEdge) {
        endY += trailingMargin;
    } else {
        escapedMargin = std::max(escapedMargin, trailingMargin);
    }

    // Your own margin always participates in parent context collapsing
    escapedMargin = std::max((double)escapedMargin, margin.bottom);

    // 5. Post-Layout Shrink-to-Fit Adjustment
    if (!hasExplicitWidth && shrinkToFit) {
        int minLeft = contentX;
        int maxRight = contentX;

        if (!box.children.empty()) {
            minLeft = INT_MAX;
            maxRight = INT_MIN;
            for (const auto& cb : box.children) {
                minLeft = std::min(minLeft, cb.x);
                maxRight = std::max(maxRight, cb.x + cb.width);
            }
        }

        int contentW = std::max(0, maxRight - minLeft);
        box.width = contentW + padding.Horizontal() + border.Horizontal();

        // FIX: Recalculate margins and x-coordinate now that we know the true width
        BoxEdges finalMargin = margin;
        ApplyMarginCentering(s, finalMargin, containerWidth, box.width);
        box.x = containerX + finalMargin.left + box.TextCenteringOffset;
        std::function<void(LayoutBox&)> removeChildPadding =
            [&](LayoutBox& child) {
                child.x -= padding.left;

                for (auto& c : child.children) {
                    removeChildPadding(c);
                }
        };
        removeChildPadding(box);

        // Update margin for debug data write-back later
        margin.left = finalMargin.left;
        margin.right = finalMargin.right;
    }

    // 6. Height Resolution
    if (s.height.unit != LengthUnit::Auto) {
        int computedContent = ResolveFromWindow(s.height, containerHeight, inheritedFontSize);
        box.height = (s.boxSizing == BoxSizing::ContentBox)
                     ? computedContent + padding.Vertical() + border.Vertical()
                     : computedContent;
        // Child margins cannot escape a fixed height boundary, but our own bottom margin still can
        escapedMargin = margin.bottom;
    } else {
        box.height = (endY - contentY) + padding.Vertical() + border.Vertical();
    }

    // Clamp constraints
    if (s.max_height.unit != LengthUnit::Auto) box.height = std::min((double)box.height, ResolveFromWindow(s.max_height, 0, inheritedFontSize));
    if (s.min_height.unit != LengthUnit::Auto) box.height = std::max((double)box.height, ResolveFromWindow(s.min_height, 0, inheritedFontSize));

    // 7. Write Back Debug Data

    auto& rd = box.node->renderData;

    rd.box = { (float)box.x, (float)box.y, (float)box.width, (float)box.height };


    rd.resolved_margin_top = margin.top; rd.resolved_margin_right = margin.right;

    rd.resolved_margin_bottom = margin.bottom; rd.resolved_margin_left = margin.left;


    rd.resolved_padding_top = padding.top;
    rd.resolved_padding_right = padding.right;
    rd.resolved_padding_bottom = padding.bottom;
    rd.resolved_padding_left = padding.left;


    return { std::move(box), (double)escapedMargin };
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
    LayoutResult result = LayoutNode(*body, 0, bodyMarginTop, renderer.GetWidth(), renderer.GetHeight());
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