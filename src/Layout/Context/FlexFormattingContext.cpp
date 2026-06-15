//
// Created by tkdtu on 6/10/2026.
//

#include "FlexFormattingContext.h"

#include <algorithm>
#include <vector>

#include "../LayoutGenerator.h"
#include "../LayoutHelper.h"

#define ResolveFromWindow(length, baseWidth, inheritedFontSize) \
    ResolveLength(length, baseWidth, lr_.GetWidth(), lr_.GetHeight(), inheritedFontSize)
static void ShiftBoxY(LayoutBox& box, int dy)
{
    box.y += dy;
    for (auto& child : box.children)
        ShiftBoxY(child, dy);
}

static void ShiftBoxX(LayoutBox& box, int dx)
{
    box.x += dx;
    for (auto& child : box.children)
        ShiftBoxX(child, dx);
}
int FlexFormattingContext::Layout(
    Node& node,
    LayoutBox& parent,
    int contentX,
    int contentY,
    int contentWidth,
    int contentHeight)
{
    const Style& style = node.computedStyle;

    const bool isRow =
        (style.flex.direction == FlexDirection::Row);

    const int availableMain =
        isRow ? contentWidth : contentHeight;

    const int availableCross =
        isRow ? contentHeight : contentWidth;

    // ─────────────────────────────────────────────────────────────
    // Collect flex items
    // ─────────────────────────────────────────────────────────────

    std::vector<FlexItem> items;

    for (auto& childPtr : node.children)
    {
        if (!childPtr)
            continue;

        Node& child = *childPtr;

        if (IsLayoutIgnored(child))
            continue;

        const Style& cs = child.computedStyle;

        int inheritedFont =
            ResolveFontSizeInherit(
                &child,
                lr_.GetWidth(),
                lr_.GetHeight());

        FlexItem item{};
        item.node = &child;

        // ─────────────────────────────────────────────────────────
        // Resolve flex basis → hypothetical main size
        // Per spec §9.2: basis wins over width/height on the main axis.
        // ─────────────────────────────────────────────────────────

        if (cs.flex.basis.unit != LengthUnit::Auto)
        {
            item.baseSize =
                ResolveFromWindow(
                    cs.flex.basis,
                    availableMain,
                    inheritedFont);
        }
        else
        {
            const CSSLength& basis =
                isRow ? cs.width : cs.height;

            if (basis.unit != LengthUnit::Auto)
            {
                item.baseSize =
                    ResolveFromWindow(
                        basis,
                        availableMain,
                        inheritedFont);
            }
            else
            {
                item.baseSize = 0;
            }
        }

        // Clamp baseSize by item's own min/max on the main axis.
        // This produces the true hypothetical main size (§9.5 step 1).
        {
            const CSSLength& minMain = isRow ? cs.min_width  : cs.min_height;
            const CSSLength& maxMain = isRow ? cs.max_width  : cs.max_height;

            if (minMain.unit != LengthUnit::Auto)
            {
                int minVal =
                    ResolveFromWindow(minMain, availableMain, inheritedFont);
                item.baseSize = std::max(item.baseSize, minVal);
            }

            if (maxMain.unit != LengthUnit::Auto)
            {
                int maxVal =
                    ResolveFromWindow(maxMain, availableMain, inheritedFont);
                item.baseSize = std::min(item.baseSize, maxVal);
            }
        }

        item.hypothetical = item.baseSize;

        item.growFactor   = cs.flex.grow;
        item.shrinkFactor = cs.flex.shrink;

        // Resolve cross-axis specified size for use during alignment.
        {
            const CSSLength& crossSpec = isRow ? cs.height : cs.width;

            if (crossSpec.unit != LengthUnit::Auto)
            {
                item.specifiedCrossSize =
                    ResolveFromWindow(crossSpec, availableCross, inheritedFont);
                item.hasCrossSize = true;
            }
            else
            {
                item.specifiedCrossSize = 0;
                item.hasCrossSize = false;
            }
        }

        items.push_back(item);
    }

    // ─────────────────────────────────────────────────────────────
    // Consume free space for margin:auto items before justify-content.
    // Per spec §8.1: auto margins absorb leftover space first.
    // ─────────────────────────────────────────────────────────────

    int totalHypothetical = 0;

    for (const auto& item : items)
        totalHypothetical += item.hypothetical;

    int freeSpace = availableMain - totalHypothetical;

    {
        int autoMarginCount = 0;

        for (const auto& item : items)
        {
            const Style& cs = item.node->computedStyle;
            if (isRow)
            {
                if (cs.margin_left.unit  == LengthUnit::Auto) autoMarginCount++;
                if (cs.margin_right.unit == LengthUnit::Auto) autoMarginCount++;
            }
            else
            {
                if (cs.margin_top.unit    == LengthUnit::Auto) autoMarginCount++;
                if (cs.margin_bottom.unit == LengthUnit::Auto) autoMarginCount++;
            }
        }

        if (autoMarginCount > 0 && freeSpace > 0)
        {
            int perAutoMargin = freeSpace / autoMarginCount;

            for (auto& item : items)
            {
                const Style& cs = item.node->computedStyle;
                item.marginMainStart = 0;
                item.marginMainEnd   = 0;

                if (isRow)
                {
                    if (cs.margin_left.unit  == LengthUnit::Auto) item.marginMainStart += perAutoMargin;
                    if (cs.margin_right.unit == LengthUnit::Auto) item.marginMainEnd   += perAutoMargin;
                }
                else
                {
                    if (cs.margin_top.unit    == LengthUnit::Auto) item.marginMainStart += perAutoMargin;
                    if (cs.margin_bottom.unit == LengthUnit::Auto) item.marginMainEnd   += perAutoMargin;
                }
            }

            // Auto margins consumed all free space; justify-content has nothing left.
            freeSpace = 0;
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Grow / Shrink  (§9.7)
    // Operates on hypothetical sizes, not raw base sizes.
    // ─────────────────────────────────────────────────────────────

    if (freeSpace > 0)
    {
        float totalGrow = 0.0f;

        for (const auto& item : items)
            totalGrow += item.growFactor;

        if (totalGrow > 0.0f)
        {
            for (auto& item : items)
            {
                float ratio = item.growFactor / totalGrow;
                item.finalMainSize =
                    item.hypothetical + (int)(freeSpace * ratio);
            }
        }
        else
        {
            for (auto& item : items)
                item.finalMainSize = item.hypothetical;
        }
    }
    else if (freeSpace < 0)
    {
        // Weighted shrink: weight = shrinkFactor * hypothetical (§9.7.4)
        float totalShrink = 0.0f;

        for (const auto& item : items)
            totalShrink += item.shrinkFactor * (float)item.hypothetical;

        if (totalShrink > 0.0f)
        {
            for (auto& item : items)
            {
                float weighted = item.shrinkFactor * (float)item.hypothetical;
                float ratio    = weighted / totalShrink;

                item.finalMainSize =
                    item.hypothetical + (int)(freeSpace * ratio);

                item.finalMainSize = std::max(0, item.finalMainSize);
            }
        }
        else
        {
            for (auto& item : items)
                item.finalMainSize = item.hypothetical;
        }
    }
    else
    {
        for (auto& item : items)
            item.finalMainSize = item.hypothetical;
    }

    // ─────────────────────────────────────────────────────────────
    // Justify-content  (§9.8)
    // Remaining space is recomputed from finalMainSize so that
    // grow/shrink rounding doesn't skew the cursor.
    // ─────────────────────────────────────────────────────────────

    int usedMain = 0;

    for (const auto& item : items)
        usedMain += item.finalMainSize + item.marginMainStart + item.marginMainEnd;

    int remaining = std::max(0, availableMain - usedMain);

    int gap        = 0;
    int cursorMain = 0;  // relative to contentX / contentY

    switch (style.flex.justifyContent)
    {
        case JustifyContent::Start:
            cursorMain = 0;
            break;

        case JustifyContent::End:
            cursorMain = remaining;
            break;

        case JustifyContent::Center:
            cursorMain = remaining / 2;
            break;

        case JustifyContent::SpaceBetween:
            cursorMain = 0;
            if (items.size() > 1)
                gap = remaining / (int)(items.size() - 1);
            break;

        case JustifyContent::SpaceAround:
            if (!items.empty())
            {
                gap        = remaining / (int)items.size();
                cursorMain = gap / 2;
            }
            break;

        case JustifyContent::SpaceEvenly:
            if (!items.empty())
            {
                gap        = remaining / (int)(items.size() + 1);
                cursorMain = gap;
            }
            break;
    }

    // ─────────────────────────────────────────────────────────────
    // Final layout pass
    // Children are laid out at their natural cross size first.
    // align-items / align-self then adjusts their cross position.
    // ─────────────────────────────────────────────────────────────

    int maxCross = availableCross;  // not 0

    for (auto& item : items)
    {
        const Style& cs = item.node->computedStyle;

        // Advance past margin-start (auto margins already baked into
        // marginMainStart; regular margins still need applying).
        cursorMain += item.marginMainStart;

        // Cross-axis: use specified size if present, otherwise natural
        // (pass availableCross only for stretch; we'll re-layout for
        // stretch after we know the line's max cross size — for now
        // pass the intrinsic/specified cross size).
        int layoutCrossSize =
            item.hasCrossSize ? item.specifiedCrossSize : 0;

        int childX, childY, childWidth, childHeight;

        if (isRow)
        {
            childX      = contentX + cursorMain;
            childY      = contentY;
            childWidth  = item.finalMainSize;
            childHeight = (layoutCrossSize > 0) ? layoutCrossSize : availableCross;
        }
        else
        {
            childX      = contentX;
            childY      = contentY + cursorMain;
            childWidth  = (layoutCrossSize > 0) ? layoutCrossSize : availableCross;
            childHeight = item.finalMainSize;
        }

        LayoutResult result =
            lr_.LayoutNode(
                *item.node,
                childX,
                childY,
                childWidth,
                childHeight);

        item.box = std::move(result.box);

        item.crossSize =
            isRow ? item.box.height : item.box.width;

        maxCross = std::max(maxCross, item.crossSize);

        cursorMain += item.finalMainSize + item.marginMainEnd + gap;
    }

    // ─────────────────────────────────────────────────────────────
    // align-items / align-self cross-axis positioning  (§9.8)
    // Now that we know maxCross (the line's cross size), reposition
    // each child on the cross axis.
    // ─────────────────────────────────────────────────────────────

    for (auto& item : items)
    {
        const Style& cs = item.node->computedStyle;

        AlignItems effectiveAlign;

        if (cs.flex.alignSelf != AlignSelf::Auto)
            effectiveAlign = static_cast<AlignItems>(cs.flex.alignSelf);
        else
            effectiveAlign = style.flex.alignItems;

        int crossOffset = 0;

        switch (effectiveAlign)
        {
            case AlignItems::FlexStart:
            case AlignItems::Start:
                crossOffset = 0;
                break;

            case AlignItems::FlexEnd:
            case AlignItems::End:
                crossOffset = maxCross - item.crossSize;
                break;

            case AlignItems::Center:
                crossOffset = (maxCross - item.crossSize) / 2;
                break;

            case AlignItems::Stretch:
                // Item already laid out with availableCross; no offset needed.
                crossOffset = 0;
                break;

            default:
                crossOffset = 0;
                break;
        }

        if (isRow)
            ShiftBoxY(item.box, crossOffset);
        else
            ShiftBoxX(item.box, crossOffset);

        parent.children.push_back(std::move(item.box));
    }

    lastChildMarginBottom_ = 0;

    // Return end of main axis content, relative to the container's origin.
    // Caller (LayoutFlex) subtracts contentY to get the content height.
    return contentY + cursorMain;
}

int FlexFormattingContext::GetLastChildMarginBottom() const
{
    return lastChildMarginBottom_;
}