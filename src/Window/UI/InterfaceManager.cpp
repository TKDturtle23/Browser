#include "InterfaceManager.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include "Platform/Platform.h"

// ---------------------------------------------------------------------------
// Construction / resize
// ---------------------------------------------------------------------------

DebugInterfaceManager::DebugInterfaceManager(int initialWidth, int initialHeight)
    : windowWidth(initialWidth), windowHeight(initialHeight),
      font("arial/ARIAL.TTF", 14)
{
    renderer = std::make_unique<Renderer>(windowWidth, windowHeight);
}

void DebugInterfaceManager::Resize(int newWidth, int newHeight) {
    windowWidth  = newWidth;
    windowHeight = newHeight;
    renderer->Resize(newWidth, newHeight);
}

const std::vector<Color>& DebugInterfaceManager::GetFrontBuffer() const {
    return renderer->GetFrontBuffer();
}

// ---------------------------------------------------------------------------
// Input injection
// ---------------------------------------------------------------------------

void DebugInterfaceManager::InjectMouseMove(int x, int y) {
    io.mouseX = x;
    io.mouseY = y;
}

void DebugInterfaceManager::InjectMouseButton(bool leftDown) {
    io.mouseLeftDown = leftDown;
}

void DebugInterfaceManager::InjectMouseWheel(int delta) {
    io.mouseWheelDelta = delta;
}

void DebugInterfaceManager::InjectKeyChar(Key key, bool shiftPressed) {
    io.backspacePressed = (key == Key::Backspace);
    io.enterPressed     = (key == Key::Return || key == Key::NumpadEnter);

    char c = '\0';

    if (key >= Key::A && key <= Key::Z) {
        int offset = static_cast<int>(key) - static_cast<int>(Key::A);
        c = shiftPressed ? ('A' + offset) : ('a' + offset);
    }
    else if (key >= Key::Num0 && key <= Key::Num9) {
        int offset = static_cast<int>(key) - static_cast<int>(Key::Num0);
        if (shiftPressed) {
            const char shiftNums[] = { ')', '!', '@', '#', '$', '%', '^', '&', '*', '(' };
            c = shiftNums[offset];
        } else {
            c = '0' + offset;
        }
    }
    else if (key >= Key::Numpad0 && key <= Key::Numpad9) {
        c = '0' + (static_cast<int>(key) - static_cast<int>(Key::Numpad0));
    }
    else {
        switch (key) {
            case Key::Space:          c = ' ';                       break;
            case Key::NumpadDivide:   c = '/';                       break;
            case Key::NumpadMultiply: c = '*';                       break;
            case Key::NumpadSubtract: c = '-';                       break;
            case Key::NumpadAdd:      c = '+';                       break;
            case Key::NumpadDecimal:  c = '.';                       break;
            case Key::Semicolon:      c = shiftPressed ? ':' : ';';  break;
            case Key::Slash:          c = shiftPressed ? '?' : '/';  break;
            case Key::Equal:          c = shiftPressed ? '+' : '=';  break;
            case Key::Hyphen:         c = shiftPressed ? '_' : '-';  break;
            case Key::LBracket:       c = shiftPressed ? '{' : '[';  break;
            case Key::RBracket:       c = shiftPressed ? '}' : ']';  break;
            case Key::Comma:          c = shiftPressed ? '<' : ',';  break;
            case Key::Period:         c = shiftPressed ? '>' : '.';  break;
            case Key::Quote:          c = shiftPressed ? '"' : '\''; break;
            case Key::Backquote:      c = shiftPressed ? '~' : '`';  break;
            case Key::Backslash:      c = shiftPressed ? '|' : '\\'; break;
            default:                  c = '\0';                      break;
        }
    }

    io.lastTypedChar = c;
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------

void DebugInterfaceManager::BeginFrame() {
    io.mouseLeftClicked = (io.mouseLeftDown && !lastMouseState);
    lastMouseState      = io.mouseLeftDown;

    renderer->Clear(Color{25, 25, 28});   // Dark panel background

    cursorX      = 5;
    cursorY      = 5;
    maxRowHeight = 0;
    hotID        = 0;

    // Wheel delta is consumed once per frame
}

void DebugInterfaceManager::EndFrame() {
    if (!io.mouseLeftDown) {
        activeID = 0;
    }

    io.lastTypedChar    = 0;
    io.backspacePressed = false;
    io.enterPressed     = false;
    io.mouseWheelDelta  = 0;

    renderer->Present();
}

// ---------------------------------------------------------------------------
// Clip rect stack
// ---------------------------------------------------------------------------

void DebugInterfaceManager::PushClipRect(int x, int y, int w, int h) {
    ClipRect incoming{x, y, w, h, true};

    if (!clipStack.empty() && clipStack.back().active) {
        // Intersect with the current top
        const ClipRect& cur = clipStack.back();
        int x2 = std::min(cur.x + cur.w, incoming.x + incoming.w);
        int y2 = std::min(cur.y + cur.h, incoming.y + incoming.h);
        incoming.x = std::max(cur.x, incoming.x);
        incoming.y = std::max(cur.y, incoming.y);
        incoming.w = std::max(0, x2 - incoming.x);
        incoming.h = std::max(0, y2 - incoming.y);
    }

    clipStack.push_back(incoming);
    activeClip = clipStack.back();
}

void DebugInterfaceManager::PopClipRect() {
    if (!clipStack.empty()) clipStack.pop_back();
    activeClip = clipStack.empty() ? ClipRect{} : clipStack.back();
}

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

void DebugInterfaceManager::SameLine(int spacing) {
    cursorX += spacing;
}

void DebugInterfaceManager::NewLine(int spacing) {
    cursorX      = 5;
    cursorY     += maxRowHeight + spacing;
    maxRowHeight = 0;
}

void DebugInterfaceManager::SetCursor(int x, int y) {
    cursorX = x;
    cursorY = y;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

UIID DebugInterfaceManager::GetID(const std::string& str) const {
    uint32_t hash = 2166136261U;
    for (char c : str) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619U;
    }
    return hash;
}

bool DebugInterfaceManager::IsMouseOver(int x, int y, int w, int h) const {
    return (io.mouseX >= x && io.mouseX < x + w &&
            io.mouseY >= y && io.mouseY < y + h);
}

bool DebugInterfaceManager::IsInsideClip(int x, int y, int w, int h) const {
    if (!activeClip.active) return true;
    return !(x + w <= activeClip.x ||
             y + h <= activeClip.y ||
             x     >= activeClip.x + activeClip.w ||
             y     >= activeClip.y + activeClip.h);
}

// Draws a string at (x, baselineY), returning the pen-X after the last glyph.
// Respects activeClip: glyphs outside the clip region are skipped.
// If maxWidth > 0 the text is truncated with "…" rather than overflow.
int DebugInterfaceManager::DrawText(const std::string& text,
                                    int x, int baselineY,
                                    Color color, int maxWidth)
{
    int penX      = x;
    char prevChar = '\0';

    // If we have a width limit, pre-calculate total text width and decide
    // whether we need to insert an ellipsis.
    bool needsEllipsis = false;
    int  ellipsisWidth = 0;
    if (maxWidth > 0) {
        auto eg = font.GetGlyph('.');
        ellipsisWidth = eg.advance * 3;

        int totalW = 0;
        for (char c : text) {
            auto g = font.GetGlyph(c);
            totalW += g.advance;
        }
        needsEllipsis = (totalW > maxWidth);
    }

    int availWidth = needsEllipsis ? (maxWidth - ellipsisWidth) : maxWidth;
    int drawnWidth = 0;

    for (char c : text) {
        auto glyph = font.GetGlyph(c);

        if (prevChar != '\0') {
            int kern = font.GetKerning(c, prevChar).x >> 6;
            penX      += kern;
            drawnWidth += kern;
        }

        if (maxWidth > 0 && drawnWidth + glyph.advance > availWidth) break;

        int gx = penX    + glyph.bearingX;
        int gy = baselineY - glyph.bearingY;

        if (IsInsideClip(gx, gy, glyph.width, glyph.height)) {
            renderer->DrawGlyph(gx, gy, glyph, color);
        }

        penX       += glyph.advance;
        drawnWidth += glyph.advance;
        prevChar    = c;
    }

    if (needsEllipsis) {
        for (int i = 0; i < 3; ++i) {
            auto glyph = font.GetGlyph('.');
            int gx = penX    + glyph.bearingX;
            int gy = baselineY - glyph.bearingY;
            if (IsInsideClip(gx, gy, glyph.width, glyph.height)) {
                renderer->DrawGlyph(gx, gy, glyph, color);
            }
            penX += glyph.advance;
        }
    }

    return penX;
}

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------

int DebugInterfaceManager::Label(const std::string& text, Color color, int maxWidth) {
    int x          = cursorX;
    int y          = cursorY;
    int baselineY  = y + 14;   // Simple fixed baseline for 14px font

    int endX = DrawText(text, x, baselineY, color, maxWidth);

    int width = endX - x;
    cursorX  += width;
    if (16 > maxRowHeight) maxRowHeight = 16;

    return width;
}

// ---------------------------------------------------------------------------
// Separator
// ---------------------------------------------------------------------------

void DebugInterfaceManager::Separator(Color color) {
    NewLine(2);
    renderer->FillRect(0, cursorY, windowWidth, 1, color);
    cursorY     += 4;
    maxRowHeight = 0;
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------

bool DebugInterfaceManager::Button(const std::string& label, int width, int height) {
    UIID id = GetID(label);
    bool clicked = false;

    int x = cursorX;
    int y = cursorY;
    cursorX += width;
    if (height > maxRowHeight) maxRowHeight = height;

    if (IsMouseOver(x, y, width, height)) {
        hotID = id;
        if (activeID == 0 && io.mouseLeftClicked) activeID = id;
    }

    Color btnColor = {55, 57, 60};
    if (hotID == id) {
        btnColor = (activeID == id) ? Color{90, 92, 95} : Color{70, 72, 75};
    }

    if (hotID == id && activeID == id && !io.mouseLeftDown) clicked = true;

    renderer->FillRectBeveled(x, y, width, height, 2, btnColor);

    int baselineY = y + (height / 2) + 5;
    DrawText(label, x + 8, baselineY, Color{210, 212, 215});

    return clicked;
}

// ---------------------------------------------------------------------------
// Tab
// ---------------------------------------------------------------------------

bool DebugInterfaceManager::Tab(const std::string& id_str, std::string& title,
                                bool isActive, int width, int height)
{
    UIID id = GetID(id_str);
    bool selected = false;

    int x = cursorX;
    int y = cursorY;
    cursorX += width;
    if (height > maxRowHeight) maxRowHeight = height;

    if (IsMouseOver(x, y, width, height)) {
        hotID = id;
        if (io.mouseLeftClicked) {
            activeID = id;
            focusID  = id;
            selected = true;
        }
    } else if (io.mouseLeftClicked && focusID == id && !isActive) {
        focusID = 0;
    }

    if (focusID == id) {
        if (io.backspacePressed && !title.empty()) title.pop_back();
        if (io.lastTypedChar >= 32 && io.lastTypedChar <= 126)
            title.push_back(io.lastTypedChar);
    }

    Color tabBg = isActive ? Color{45, 47, 50} : Color{32, 34, 37};
    if (!isActive && hotID == id) tabBg = Color{38, 40, 43};
    renderer->FillRectBeveled(x, y, width, height, 3, tabBg);

    // Active tab: accent bar along the top
    if (isActive) {
        renderer->FillRect(x, y, width, 2, Color{88, 166, 255});
    }

    int baselineY = y + (height / 2) + 5;
    DrawText(title, x + 8, baselineY, Color{200, 202, 205}, width - 16);

    return selected || (focusID == id && io.enterPressed);
}

// ---------------------------------------------------------------------------
// Checkbox
// ---------------------------------------------------------------------------

bool DebugInterfaceManager::Checkbox(const std::string& id_str,
                                     const std::string& label, bool& checked)
{
    UIID id = GetID(id_str);
    bool changed = false;

    const int boxSize = 14;
    int x = cursorX;
    int y = cursorY;

    if (IsMouseOver(x, y, boxSize, boxSize)) {
        hotID = id;
        if (activeID == 0 && io.mouseLeftClicked) activeID = id;
    }

    if (hotID == id && activeID == id && !io.mouseLeftDown) {
        checked = !checked;
        changed  = true;
    }

    Color boxBg = checked ? Color{88, 166, 255} : Color{55, 57, 60};
    renderer->FillRectBeveled(x, y, boxSize, boxSize, 2, boxBg);

    if (checked) {
        // Simple tick: two small rects forming a checkmark shape
        renderer->FillRect(x + 3, y + 7,  4, 2, Color{255, 255, 255});
        renderer->FillRect(x + 6, y + 4,  2, 5, Color{255, 255, 255});
    }

    cursorX += boxSize + 5;
    if (boxSize > maxRowHeight) maxRowHeight = boxSize;

    DrawText(label, cursorX, y + boxSize - 2, Color{200, 202, 205});

    // Advance cursor past label
    int labelW = 0;
    for (char c : label) labelW += font.GetGlyph(c).advance;
    cursorX += labelW;

    return changed;
}

// ---------------------------------------------------------------------------
// ColorBadge
// ---------------------------------------------------------------------------

void DebugInterfaceManager::ColorBadge(const std::string& label,
                                       Color bg, Color fg,
                                       int paddingX, int paddingH)
{
    int textW = 0;
    for (char c : label) textW += font.GetGlyph(c).advance;

    int badgeW = textW + paddingX * 2;
    int badgeH = 14 + paddingH * 2;

    int x = cursorX;
    int y = cursorY;

    renderer->FillRectBeveled(x, y, badgeW, badgeH, 3, bg);

    int baselineY = y + badgeH / 2 + 5;
    DrawText(label, x + paddingX, baselineY, fg);

    cursorX += badgeW;
    if (badgeH > maxRowHeight) maxRowHeight = badgeH;
}

// ---------------------------------------------------------------------------
// TextField / AddressBar shared guts
// ---------------------------------------------------------------------------

bool DebugInterfaceManager::TextInputWidget(UIID id,
                                            std::string& text,
                                            int x, int y,
                                            int width, int height,
                                            Color focusBorderColor)
{
    TextFieldState& s = textFieldStates[id];

    bool mouseOver = IsMouseOver(x, y, width, height);

    if (mouseOver) {
        hotID = id;
        if (io.mouseLeftClicked) {
            focusID    = id;
            s.isDragging       = true;
            s.selectStartIndex = -1;
        }
    } else if (io.mouseLeftClicked && focusID == id) {
        focusID    = 0;
        s.isDragging = false;
    }

    if (!io.mouseLeftDown) s.isDragging = false;

    if (focusID == id) {
        bool hasSel = (s.selectStartIndex != -1 && s.selectStartIndex != s.cursorIndex);
        int selMin  = hasSel ? std::min(s.selectStartIndex, s.cursorIndex) : 0;
        int selMax  = hasSel ? std::max(s.selectStartIndex, s.cursorIndex) : 0;

        if (io.backspacePressed) {
            if (hasSel) {
                text.erase(selMin, selMax - selMin);
                s.cursorIndex = selMin;
                s.selectStartIndex = s.cursorIndex;
            } else if (s.cursorIndex > 0) {
                text.erase(s.cursorIndex - 1, 1);
                s.cursorIndex--;
                s.selectStartIndex = s.cursorIndex;
            }
        } else if (io.lastTypedChar >= 32 && io.lastTypedChar <= 126) {
            if (hasSel) {
                text.erase(selMin, selMax - selMin);
                s.cursorIndex = selMin;
            }
            text.insert(text.begin() + s.cursorIndex, io.lastTypedChar);
            s.cursorIndex++;
            s.selectStartIndex = s.cursorIndex;
        }

        s.cursorIndex = std::clamp(s.cursorIndex, 0, (int)text.size());
    }

    // Pre-calculate char X positions
    std::vector<int> charX;
    charX.reserve(text.size() + 1);
    int penX     = x + 8;
    char prevChar = '\0';
    charX.push_back(penX);
    s.cursorIndex = std::clamp(s.cursorIndex, 0, (int)text.size());
    s.selectStartIndex = std::clamp(s.selectStartIndex, 0, (int)text.size());
    for (char c : text) {
        auto g = font.GetGlyph(c);
        if (prevChar != '\0') penX += font.GetKerning(c, prevChar).x >> 6;
        penX += g.advance;
        charX.push_back(penX);
        prevChar = c;
    }

    // Mouse picking
    if (focusID == id && (io.mouseLeftClicked || s.isDragging)) {
        int target = 0, minDist = 999999;
        for (size_t i = 0; i < charX.size(); ++i) {
            int d = std::abs(io.mouseX - charX[i]);
            if (d < minDist) { minDist = d; target = (int)i; }
        }
        if (io.mouseLeftClicked) {
            s.cursorIndex      = target;
            s.selectStartIndex = target;
        } else {
            s.cursorIndex = target;
        }
    }

    // Draw background + border
    Color borderColor = (focusID == id) ? focusBorderColor : Color{70, 72, 75};
    renderer->FillRectWithBorder(x, y, width, height, Color{38, 40, 43}, borderColor);

    // Selection highlight
    bool hasSel = (focusID == id &&
                   s.selectStartIndex != -1 &&
                   s.selectStartIndex != s.cursorIndex);
    int selMin = hasSel ? std::min(s.selectStartIndex, s.cursorIndex) : 0;
    int selMax = hasSel ? std::max(s.selectStartIndex, s.cursorIndex) : 0;

    if (hasSel) {
        int hx = charX[selMin];
        int hw = charX[selMax] - hx;
        renderer->FillRect(hx, y + 3, hw, height - 6, Color{40, 90, 160});
    }

    // Draw text glyphs
    int drawPenX = x + 8;
    prevChar = '\0';
    int baselineY = y + (height / 2) + 5;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        auto g = font.GetGlyph(c);
        if (g.width == 0 && c != ' ') { drawPenX += g.advance; continue; }
        if (prevChar != '\0') drawPenX += font.GetKerning(c, prevChar).x >> 6;

        Color tc = (hasSel && (int)i >= selMin && (int)i < selMax)
                   ? Color{180, 210, 255} : Color{210, 212, 215};

        renderer->DrawGlyph(drawPenX + g.bearingX, baselineY - g.bearingY, g, tc);
        drawPenX += g.advance;
        prevChar  = c;
    }

    // Cursor bar
    if (focusID == id && s.cursorIndex < (int)charX.size()) {
        renderer->FillRect(charX[s.cursorIndex], y + 3, 2, height - 6, Color{180, 210, 255});
    }

    return (focusID == id && io.enterPressed);
}

bool DebugInterfaceManager::TextField(const std::string& id_str,
                                      std::string& text,
                                      int width, int height)
{
    UIID id = GetID(id_str);
    int x = cursorX, y = cursorY;
    cursorX += width;
    if (height > maxRowHeight) maxRowHeight = height;
    return TextInputWidget(id, text, x, y, width, height, Color{88, 166, 255});
}

bool DebugInterfaceManager::AddressBar(const std::string& id_str,
                                       std::string& text,
                                       int width, int height)
{
    UIID id = GetID(id_str);
    int x = cursorX, y = cursorY;
    cursorX += width;
    if (height > maxRowHeight) maxRowHeight = height;
    return TextInputWidget(id, text, x, y, width, height, Color{66, 133, 244});
}

// ---------------------------------------------------------------------------
// ScrollableList
// ---------------------------------------------------------------------------

int DebugInterfaceManager::ScrollableList(const std::string& id_str,
                                          std::vector<ListRow>& rows,
                                          int width, int height, bool flip,
                                          int rowHeight)
{
    UIID id  = GetID(id_str);
    auto& ls = listStates[id];

    int x = cursorX;
    int y = cursorY;
    cursorX += width;
    if (height > maxRowHeight) maxRowHeight = height;

    // Total content height
    int totalH = (int)rows.size() * rowHeight;

    // Clamp scroll so we never over-scroll
    int maxScroll = std::max(0, totalH - height);
    ls.scrollOffsetY = std::clamp(ls.scrollOffsetY, 0, maxScroll);

    // --- Mouse wheel scroll (only when hovered) ---
    if (IsMouseOver(x, y, width, height) && io.mouseWheelDelta != 0) {
        // Flipping scroll direction contextually feels more intuitive if kept standard,
        // but if you want wheel down to always mean "move content up", leave this unchanged.
        ls.scrollOffsetY -= io.mouseWheelDelta * rowHeight * 3;
        ls.scrollOffsetY  = std::clamp(ls.scrollOffsetY, 0, maxScroll);
    }

// --- Click detection: which row was hit? ---
int clickedRow = -1;
if (IsMouseOver(x, y, width, height) && io.mouseLeftClicked) {
    int hiddenIndentThreshold = -1;
    int visualRowIndex = 0;

    // First, count total visible rows if we need it for the flip math
    int totalVisibleRows = 0;
    if (flip) {
        int tempHiddenThreshold = -1;
        for (const auto& row : rows) {
            if (tempHiddenThreshold != -1) {
                if (row.IndentLevel > tempHiddenThreshold) continue;
                else tempHiddenThreshold = -1;
            }
            if (row.isCollapsed) tempHiddenThreshold = row.IndentLevel;
            totalVisibleRows++;
        }
    }

    for (int i = 0; i < (int)rows.size(); ++i) {
        auto& row = rows[i];

        // 1. COLLAPSING LOGIC: Skip hidden rows completely
        if (hiddenIndentThreshold != -1) {
            if (row.IndentLevel > hiddenIndentThreshold) {
                continue;
            } else {
                hiddenIndentThreshold = -1;
            }
        }
        if (row.isCollapsed) {
            hiddenIndentThreshold = row.IndentLevel;
        }

        // 2. Y-COORDINATE CALCULATION
        int rowY = 0;
        if (flip) {
            int rowFromBottom = (totalVisibleRows - 1) - visualRowIndex;
            rowY = (y + height) - (rowFromBottom * rowHeight) + ls.scrollOffsetY - rowHeight;
        } else {
            rowY = y + (visualRowIndex * rowHeight) - ls.scrollOffsetY;
        }

        // 3. HIT DETECTION
        if (io.mouseY >= rowY && io.mouseY < (rowY + rowHeight)) {
            int indentWidth = row.IndentLevel * 16;
            int toggleLeft  = x + 6 + indentWidth;
            int toggleWidth = 12;

            bool hasChildren = (rows.size() > i + 1) && (rows[i + 1].IndentLevel > row.IndentLevel);

            if (hasChildren && io.mouseX >= toggleLeft && io.mouseX <= (toggleLeft + toggleWidth)) {
                row.isCollapsed = !row.isCollapsed;
            } else {
                ls.selectedRow = i;
                clickedRow     = i;
            }
            break;
        }

        visualRowIndex++;
    }
}
    // --- Draw background ---
renderer->FillRect(x, y, width, height, Color{28, 30, 33});

// --- Push clip rect so rows don't bleed outside the viewport ---
PushClipRect(x, y, width, height);

// --- Draw rows ---
int hiddenIndentThreshold = -1;
int visualRowIndex = 0;

// Count total visible rows first if 'flip' logic requires it
int totalVisibleRows = 0;
if (flip) {
    int tempHiddenThreshold = -1;
    for (const auto& r : rows) {
        if (tempHiddenThreshold != -1) {
            if (r.IndentLevel > tempHiddenThreshold) continue;
            else tempHiddenThreshold = -1;
        }
        if (r.isCollapsed) tempHiddenThreshold = r.IndentLevel;
        totalVisibleRows++;
    }
}

for (int i = 0; i < (int)rows.size(); ++i) {
    ListRow& row = rows[i];

    // --- 1. COLLAPSING LOGIC ---
    if (hiddenIndentThreshold != -1) {
        if (row.IndentLevel > hiddenIndentThreshold) {
            continue; // Skip rendering and don't increment visualRowIndex
        } else {
            hiddenIndentThreshold = -1;
        }
    }

    if (row.isCollapsed) {
        hiddenIndentThreshold = row.IndentLevel;
    }

    // --- Y COORDINATE MATH ---
    int rowY = 0;
    if (flip) {
        int rowFromBottom = (totalVisibleRows - 1) - visualRowIndex;
        rowY = (y + height) - (rowFromBottom * rowHeight) + ls.scrollOffsetY - rowHeight;
    } else {
        rowY = y + (visualRowIndex * rowHeight) - ls.scrollOffsetY;
    }

    // Advance visual counter immediately for the next visible item
    visualRowIndex++;

    // --- FRUSTUM CULLING (Optimization) ---
    // Skip rendering calculations if the row is completely off-screen vertically
    if (rowY + rowHeight < y || rowY > y + height) {
        continue;
    }

    // --- BACKGROUND RENDERING ---
    Color rowBg = (ls.selectedRow == i) ? Color{50, 80, 130} : ((i % 2 == 1) ? Color{32, 34, 37} : Color{28, 30, 33});
    if (row.hasTint) {
        rowBg.r = (rowBg.r + row.rowTint.r) / 2;
        rowBg.g = (rowBg.g + row.rowTint.g) / 2;
        rowBg.b = (rowBg.b + row.rowTint.b) / 2;
    }

    renderer->FillRect(x, rowY, width, rowHeight, rowBg);

    if (ls.selectedRow != i && IsMouseOver(x, rowY, width, rowHeight)) {
        renderer->FillRect(x, rowY, width, rowHeight, Color{42, 44, 47});
    }

    // --- 2. INDENTATION LOGIC ---
    int indentWidth = row.IndentLevel * 16;
    int contentX = x + 6 + indentWidth;
    int baselineY = rowY + rowHeight / 2 + 5;

    bool hasChildren = (rows.size() > i + 1) && (rows[i + 1].IndentLevel > row.IndentLevel);
    if (hasChildren) {
        std::string toggleIcon = row.isCollapsed ? ">" : "v";
        DrawText(toggleIcon, contentX, baselineY, Color{150, 152, 155});
        contentX += 12;
    }

    // --- TEXT RENDERING ---
    if (!row.badgeLabel.empty()) {
        int bw = 0; for (char c : row.badgeLabel) bw += font.GetGlyph(c).advance; bw += 8;
        renderer->FillRectBeveled(contentX, rowY + 2, bw, rowHeight - 4, 2, row.badgeColor);
        DrawText(row.badgeLabel, contentX + 4, baselineY, row.badgeText);
        contentX += bw + 6;
    }

    int annotW = 0;
    if (!row.annotation.empty()) {
        for (char c : row.annotation) annotW += font.GetGlyph(c).advance; annotW += 12;
    }

    int textMaxW = (x + width - annotW) - contentX - 4;
    if (textMaxW > 0) {
        DrawText(row.text, contentX, baselineY, row.TextColor, textMaxW);
    }

    if (!row.annotation.empty()) {
        DrawText(row.annotation, x + width - annotW, baselineY, row.AnnotationColor);
    }
}

PopClipRect();

    // --- Scrollbar ---
    if (totalH > height) {
        const int sbW = 5;
        int sbX      = x + width - sbW;
        float ratio  = (float)height / (float)totalH;
        int   sbH    = std::max(20, (int)(height * ratio));
        float scroll = (float)ls.scrollOffsetY / (float)maxScroll;

        int sbY = 0;
        if (flip) {
            // ScrollOffset 0 means we are tracking the newest items at the bottom.
            // Scrollbar handle should be at the bottom.
            sbY = (y + height - sbH) - (int)((height - sbH) * scroll);
        } else {
            sbY = y + (int)((height - sbH) * scroll);
        }

        renderer->FillRect(sbX, y,   sbW, height, Color{35, 37, 40});
        renderer->FillRect(sbX, sbY, sbW, sbH,    Color{90, 92, 95});
    }

    // Border around the whole list
    renderer->DrawRect(x, y, width, height, Color{55, 57, 60});

    return clickedRow;
}

int DebugInterfaceManager::GetListSelection(const std::string& id_str) {
    UIID id = GetID(id_str);
    auto it = listStates.find(id);
    return (it != listStates.end()) ? it->second.selectedRow : -1;
}

void DebugInterfaceManager::ClearListSelection(const std::string& id_str) {
    UIID id = GetID(id_str);
    listStates[id].selectedRow = -1;
}

void DebugInterfaceManager::ScrollListToBottom(const std::string& id_str,
                                               const std::vector<ListRow>& rows,
                                               int viewportHeight, int rowHeight)
{
    UIID id = GetID(id_str);
    int totalH   = (int)rows.size() * rowHeight;
    int maxScroll = std::max(0, totalH - viewportHeight);
    listStates[id].scrollOffsetY = maxScroll;
}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

int DebugInterfaceManager::BeginPanel(int x, int y, int width, int height,
                                      Color bg, int paddingX, int paddingY)
{
    renderer->FillRect(x, y, width, height, bg);
    renderer->DrawRect(x, y, width, height, Color{55, 57, 60});
    SetCursor(x + paddingX, y + paddingY);
    maxRowHeight = 0;
    return width - paddingX * 2;
}

void DebugInterfaceManager::EndPanel() {
    // Reserved for future auto-pop behaviour
}