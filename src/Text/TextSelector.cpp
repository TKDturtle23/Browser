#include "TextSelector.h"
#include <climits>
#include <algorithm>
#include <functional>

#include "Platform/Platform.h"

namespace Engine::UI {

LayoutBox* TextSelector::SnapToNearestRun(LayoutBox& root, int mx, int my) {
    LayoutBox* best = nullptr;
    int bestDist = INT_MAX;

    std::function<void(LayoutBox&)> walk = [&](LayoutBox& box) {
        if (box.kind == BoxKind::TextRun && !box.text.chars.empty()) {
            int dx = std::max(0, std::max(box.x - mx, mx - (box.x + box.width)));
            int dy = std::max(0, std::max(box.y - my, my - (box.y + box.height)));
            int dist = dx + dy * 4; 
            if (dist < bestDist) { bestDist = dist; best = &box; }
        }
        for (auto& child : box.children) walk(child);
    };
    walk(root);
    return best;
}
    static TextHitResult NormalizeHit(
    LayoutBox& root,
    LayoutRenderer& renderer,
    int x,
    int y
) {
    auto hit = renderer.HitTestTextPosition(root, x, y);

    if (hit.valid)
        return hit;

    if (auto* run = TextSelector::SnapToNearestRun(root, x, y)) {

        hit.box = run;

        hit.offset =
            (x < run->x + run->width / 2)
            ? 0
            : static_cast<int>(run->text.chars.size());

        hit.valid = true;
    }

    return hit;
}
static bool IsBefore(
    LayoutBox& root,
    const TextPosition& a,
    const TextPosition& b
) {
    if (a.box == b.box)
        return a.offset < b.offset;

    bool foundA = false;
    bool foundB = false;

    std::function<bool(LayoutBox&)> walk =
        [&](LayoutBox& box) -> bool
    {
        if (&box == a.box) {
            foundA = true;
            return foundB;
        }

        if (&box == b.box) {
            foundB = true;
            return foundA;
        }

        for (auto& child : box.children) {
            if (walk(child))
                return true;
        }

        return false;
    };

    return walk(root) && foundA;
}


void TextSelector::UpdateAndApplySelection(
    LayoutBox& root,
    const ViewportIO& io,
    PersistentSelection& selection,
    LayoutRenderer& renderer,
    Platform* platform
) {

    //
    // Hover Cursor
    //
    {
        auto hover =
            renderer.HitTestTextPosition(
                root,
                io.mouse_x,
                io.mouse_y
            );

        platform->SetCursorType(
            hover.valid
                ? CursorType::IBeam
                : CursorType::Arrow
        );
    }

    //
    // Selection Drag
    //
    if (io.is_dragging) {

        auto startHit = NormalizeHit(
            root,
            renderer,
            io.mouse_drag_x,
            io.mouse_drag_y
        );

        auto endHit = NormalizeHit(
            root,
            renderer,
            io.mouse_x,
            io.mouse_y
        );

        if (startHit.valid && endHit.valid) {

            selection.active = true;

            //
            // anchor/focus model
            //
            selection.start = startHit;
            selection.end   = endHit;
        }
    }

    //
    // Clear old highlights
    //
    std::function<void(LayoutBox&)> clear =
        [&](LayoutBox& box)
    {
        if (box.kind == BoxKind::TextRun) {

            for (auto& c : box.text.chars) {
                c.highlighted = false;
            }
        }

        for (auto& child : box.children) {
            clear(child);
        }
    };

    clear(root);

    if (!selection.active)
        return;

    //
    // Normalize order
    //
    auto start = selection.start;
    auto end   = selection.end;

    bool swapped = false;

    if (start.box == end.box) {

        swapped = start.offset > end.offset;
    }
    else {

        bool foundEndFirst = false;

        std::function<bool(LayoutBox&)> walk =
            [&](LayoutBox& box) -> bool
        {
            if (&box == end.box) {
                foundEndFirst = true;
                return true;
            }

            if (&box == start.box) {
                foundEndFirst = false;
                return true;
            }

            for (auto& child : box.children) {
                if (walk(child))
                    return true;
            }

            return false;
        };

        walk(root);

        swapped = foundEndFirst;
    }

    if (swapped) {
        std::swap(start, end);
    }

    //
    // Empty selection = caret state
    //
    bool collapsed =
        start.box == end.box &&
        start.offset == end.offset;

    //
    // Apply selection
    //
    bool inside = false;
    bool done = false;

    std::function<void(LayoutBox&)> apply =
        [&](LayoutBox& box)
    {
        if (done)
            return;

        if (box.kind == BoxKind::TextRun) {

            bool isStart = (&box == start.box);
            bool isEnd   = (&box == end.box);

            if (isStart)
                inside = true;

            if (inside && !collapsed) {

                auto& chars = box.text.chars;

                for (int i = 0;
                     i < (int)chars.size();
                     i++)
                {
                    bool selected = true;

                    if (isStart && i < start.offset)
                        selected = false;

                    if (isEnd && i >= end.offset)
                        selected = false;

                    chars[i].highlighted = selected;
                }
            }

            if (isEnd) {
                done = true;
                return;
            }
        }

        for (auto& child : box.children) {
            apply(child);
        }
    };

    apply(root);
}
}
