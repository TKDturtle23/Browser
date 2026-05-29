#include "InterfaceManager.h"

#include <iostream>

#include "../../Platform/Platform.h"
InterfaceManager::InterfaceManager(int initialWidth, int initialHeight)
    : windowWidth(initialWidth), windowHeight(initialHeight), cursorX(0), cursorY(0), maxRowHeight(0), font("arial/ARIAL.TTF", 16) {
    renderer = std::make_unique<Renderer>(windowWidth, windowHeight);

}

void InterfaceManager::Resize(int newWidth, int newHeight) {
    windowWidth = newWidth;
    windowHeight = newHeight;
    renderer->Resize(newWidth, newHeight);
}

const std::vector<Color>& InterfaceManager::GetFrontBuffer() const {
    return renderer->GetFrontBuffer();
}

// --- Input Processing Injection ---
void InterfaceManager::InjectMouseMove(int x, int y) {
    io.mouseX = x;
    io.mouseY = y;
}

void InterfaceManager::InjectMouseButton(bool leftDown) {
    io.mouseLeftDown = leftDown;
}

void InterfaceManager::InjectKeyChar(Key key, bool shiftPressed = false) {
    // 1. Handle special action flags
    io.backspacePressed = (key == Key::Backspace);
    io.enterPressed     = (key == Key::Return || key == Key::NumpadEnter);

    // 2. Map engine Key back to a printable char
    char c = '\0';

    // Contiguous blocks
    if (key >= Key::A && key <= Key::Z) {
        int offset = static_cast<int>(key) - static_cast<int>(Key::A);
        c = shiftPressed ? ('A' + offset) : ('a' + offset);
    }
    else if (key >= Key::Num0 && key <= Key::Num9) {
        int offset = static_cast<int>(key) - static_cast<int>(Key::Num0);
        if (shiftPressed) {
            // Standard US Keyboard Shift+Number mappings
            const char shiftNumbers[] = { ')', '!', '@', '#', '$', '%', '^', '&', '*', '(' };
            c = shiftNumbers[offset];
        } else {
            c = '0' + offset;
        }
    }
    else if (key >= Key::Numpad0 && key <= Key::Numpad9) {
        c = '0' + (static_cast<int>(key) - static_cast<int>(Key::Numpad0));
    }
    // Individual miscellaneous & punctuation keys
    else {
        switch (key) {
            case Key::Space:          c = ' '; break;

            // Numpad Math
            case Key::NumpadDivide:   c = '/'; break;
            case Key::NumpadMultiply: c = '*'; break;
            case Key::NumpadSubtract: c = '-'; break;
            case Key::NumpadAdd:      c = '+'; break;
            case Key::NumpadDecimal:  c = '.'; break;

            // Punctuation (US Layout defaults)
            case Key::Semicolon:      c = shiftPressed ? ':' : ';'; break;
            case Key::Slash:          c = shiftPressed ? '?' : '/'; break;
            case Key::Equal:          c = shiftPressed ? '+' : '='; break;
            case Key::Hyphen:         c = shiftPressed ? '_' : '-'; break;
            case Key::LBracket:       c = shiftPressed ? '{' : '['; break;
            case Key::RBracket:       c = shiftPressed ? '}' : ']'; break;
            case Key::Comma:          c = shiftPressed ? '<' : ','; break;
            case Key::Period:         c = shiftPressed ? '>' : '.'; break;
            case Key::Quote:          c = shiftPressed ? '"' : '\''; break;
            case Key::Backquote:      c = shiftPressed ? '~' : '`'; break;
            case Key::Backslash:      c = shiftPressed ? '|' : '\\'; break;

            default:                  c = '\0'; break; // Non-printable (F1, Ctrl, Arrows, etc.)
        }
    }

    io.lastTypedChar = c;
}

// --- IMUI Lifecycle Frame Setup ---
void InterfaceManager::BeginFrame() {
    // 1. Calculate click edge trigger (down this frame, but wasn't down last frame)
    io.mouseLeftClicked = (io.mouseLeftDown && !lastMouseState);
    lastMouseState = io.mouseLeftDown;

    // 2. Clear out the background canvas
    renderer->Clear(Color{220, 222, 225});

    // 3. Reset the layout sequence cursor
    cursorX = 5;
    cursorY = 5;
    maxRowHeight = 0;

    // Reset hot element transient state (will re-assert during widget evaluation calls)
    hotID = 0;
}

void InterfaceManager::EndFrame() {
    // If the mouse was released, clear the active status
    if (!io.mouseLeftDown) {
        activeID = 0;
    }

    // Clear transient one-shot keyboard triggers
    io.lastTypedChar = 0;
    io.backspacePressed = false;
    io.enterPressed = false;

    // Swap our buffers to present the drawn UI
    renderer->Present();
}

// --- Layout Modifiers ---
void InterfaceManager::SameLine(int spacing) {
    cursorX += spacing;
}

void InterfaceManager::NewLine(int spacing) {
    cursorX = 5;
    cursorY += maxRowHeight + spacing;
    maxRowHeight = 0;
}

void InterfaceManager::SetCursor(int x, int y) {
    cursorX = x;
    cursorY = y;
}

// --- Immediate Mode UI Widget Implementations ---

bool InterfaceManager::Button(const std::string& label, int width, int height) {
    UIID id = GetID(label);
    bool clicked = false;

    // Update Layout Tracking mechanics
    int x = cursorX;
    int y = cursorY;
    cursorX += width;
    if (height > maxRowHeight) maxRowHeight = height;

    // Check interaction boundaries
    if (IsMouseOver(x, y, width, height)) {
        hotID = id;
        if (activeID == 0 && io.mouseLeftClicked) {
            activeID = id;
        }
    }

    // Determine visual style color scheme based on interaction state
    Color btnColor{180, 182, 185}; // Default normal
    if (hotID == id) {
        btnColor = (activeID == id) ? Color{140, 142, 145} : Color{200, 202, 205}; // Clicked vs Hovered
    }

    // Act on logic if released safely inside the button boundary
    if (hotID == id && activeID == id && !io.mouseLeftDown) {
        clicked = true;
    }

    // Render step
    renderer->FillRectBeveled(x, y, width, height, 2, btnColor);

    // 1. Establish a solid baseline Y coordinate relative to the widget's top edge
    int baselineY = y + (height / 2) + 4;

    // 2. Start drawing a bit inside the left edge of the button
    int penX = x + 8;

    char prevChar = '\0';
    for (char c : label) {
        auto glyph = font.GetGlyph(c);

        // Apply kerning (GetKerning returns raw 26.6 fractional units, so shift this!)
        if (prevChar != '\0') {
            penX += (font.GetKerning(c, prevChar).x >> 6);
        }

        // Do NOT shift bearingX or bearingY (bitmap_left/top are already standard pixels)
        int glyphLeft = penX + glyph.bearingX;
        int glyphTop = baselineY - glyph.bearingY;

        // Render the actual glyph pixels
        renderer->DrawGlyph(glyphLeft, glyphTop, glyph, Color{255, 255, 255});

        // Do NOT shift advance here (it was already shifted right by 6 inside Font::LoadGlyph!)
        penX += glyph.advance;

        prevChar = c;
    }

    return clicked;
}

bool InterfaceManager::Tab(const std::string& id_str, std::string& title, bool isActive, int width, int height) {
    // 1. Use the static id_str so the UIID remains stable while typing!
    UIID id = GetID(id_str);
    bool selected = false;

    int x = cursorX;
    int y = cursorY;
    cursorX += width;
    if (height > maxRowHeight) maxRowHeight = height;

    // Check interaction boundaries
    if (IsMouseOver(x, y, width, height)) {
        hotID = id;
        if (io.mouseLeftClicked) {
            activeID = id;
            selected = true;
            focusID = id; // Grab keyboard focus
        }
    } else if (io.mouseLeftClicked) {
        if (focusID == id && !isActive) focusID = 0;
    }

    // 2. Handle real-time text input adjustments
    if (focusID == id) {
        if (io.backspacePressed && !title.empty()) {
            title.pop_back();
        }
        if (io.lastTypedChar >= 32 && io.lastTypedChar <= 126) {
            title.push_back(io.lastTypedChar);
        }
    }

    // Background Render
    Color tabColor = isActive ? Color{255, 255, 255} : Color{170, 172, 175};
    if (!isActive && hotID == id) {
        tabColor = Color{195, 197, 200};
    }
    renderer->FillRectBeveled(x, y, width, height, 3, tabColor);

    // Text Render Loop
    int baselineY = y + (height / 2) + 5;
    int penX = x + 10;
    char prevChar = '\0';
    Color textColor{0, 0, 0};

    for (char c : title) {
        auto glyph = font.GetGlyph(c);
        if (glyph.width == 0 && c != ' ') continue;

        if (prevChar != '\0') {
            penX += (font.GetKerning(c, prevChar).x >> 6);
        }

        int glyphLeft = penX + glyph.bearingX;
        int glyphTop = baselineY - glyph.bearingY;

        renderer->DrawGlyph(glyphLeft, glyphTop, glyph, textColor);

        penX += glyph.advance;
        prevChar = c;
    }

    return selected || (focusID == id && io.enterPressed);
}
bool InterfaceManager::AddressBar(const std::string& id_str, std::string& text, int width, int height) {
    UIID id = GetID(id_str);

    int x = cursorX;
    int y = cursorY;
    cursorX += width;
    if (height > maxRowHeight) maxRowHeight = height;

    bool mouseOver = IsMouseOver(x, y, width, height);

    // --- 1. Interaction Detection & Mouse Selection Setup ---
    if (mouseOver) {
        hotID = id;
        if (io.mouseLeftClicked) {
            focusID = id;
            isDraggingText = true;
            selectStartIndex = -1; // Reset selection initially on click
        }
    } else if (io.mouseLeftClicked) {
        if (focusID == id) {
            focusID = 0; // Drop focus
            isDraggingText = false;
        }
    }

    if (!io.mouseLeftDown) {
        isDraggingText = false; // Stop dragging selection when mouse is lifted
    }

    // --- 2. Keyboard Input Processing ---
    if (focusID == id) {
        bool hasSelection = (selectStartIndex != -1 && selectStartIndex != cursorIndex);
        int selMin = hasSelection ? std::min(selectStartIndex, cursorIndex) : 0;
        int selMax = hasSelection ? std::max(selectStartIndex, cursorIndex) : 0;

        // Handle Backspace
        if (io.backspacePressed) {
            if (hasSelection) {
                // Delete the entire highlighted selection block
                text.erase(selMin, selMax - selMin);
                cursorIndex = selMin;
                selectStartIndex = cursorIndex; // FIX: Collapse selection to the new cursor position
            } else if (cursorIndex > 0) {
                // Delete a single character behind the cursor
                text.erase(cursorIndex - 1, 1);
                cursorIndex--;
                selectStartIndex = cursorIndex; // FIX: Keep selection start locked to the cursor
            }
        }
        // Handle Typing Characters
        else if (io.lastTypedChar >= 32 && io.lastTypedChar <= 126) {
            if (hasSelection) {
                text.erase(selMin, selMax - selMin);
                cursorIndex = selMin;
            }
            text.insert(text.begin() + cursorIndex, io.lastTypedChar);
            cursorIndex++;
            selectStartIndex = cursorIndex; // FIX: Keep selection start locked to the cursor
        }

        // Safety clamps
        if (cursorIndex < 0) cursorIndex = 0;
        if (cursorIndex > (int)text.size()) cursorIndex = (int)text.size();
    }

    // --- 3. Geometric Metric Pre-calculation ---
    // We pre-calculate character pixel offsets to enable precise mouse picking and highlighting layout
    int baselineY = y + (height / 2) + 5;
    int startPenX = x + 10;

    std::vector<int> charXPositions; // Holds pixel-X position of every character boundary
    charXPositions.push_back(startPenX);

    int penX = startPenX;
    char prevChar = '\0';
    for (char c : text) {
        auto glyph = font.GetGlyph(c);
        if (prevChar != '\0') {
            penX += (font.GetKerning(c, prevChar).x >> 6);
        }
        penX += glyph.advance;
        charXPositions.push_back(penX);
        prevChar = c;
    }

    // --- 4. Process Mouse Dragging and Cursor Picking ---
    if (focusID == id && (io.mouseLeftClicked || isDraggingText)) {
        // Find closest character boundary index to the mouse position
        int targetIndex = 0;
        int minDistance = 999999;
        for (size_t i = 0; i < charXPositions.size(); ++i) {
            int dist = std::abs(io.mouseX - charXPositions[i]);
            if (dist < minDistance) {
                minDistance = dist;
                targetIndex = i;
            }
        }

        if (io.mouseLeftClicked) {
            cursorIndex = targetIndex;
            selectStartIndex = targetIndex; // Selection baseline starts here
        } else if (isDraggingText) {
            cursorIndex = targetIndex; // Update terminal cursor while dragging
        }
    }

    // --- 5. Rendering Steps ---
    Color borderColors = (focusID == id) ? Color{66, 133, 244} : Color{140, 142, 145};
    renderer->FillRectWithBorder(x, y, width, height, Color{255, 255, 255}, borderColors);

    // Contextual selection check variables
    bool hasSelection = (focusID == id && selectStartIndex != -1 && selectStartIndex != cursorIndex);
    int selMin = hasSelection ? std::min(selectStartIndex, cursorIndex) : 0;
    int selMax = hasSelection ? std::max(selectStartIndex, cursorIndex) : 0;

    // Render Selection Highlight Box background layer
    if (hasSelection) {
        int highlightX = charXPositions[selMin];
        int highlightW = charXPositions[selMax] - highlightX;
        // Draw selection backing (Classic blue color layer)
        renderer->FillRect(highlightX, y + 4, highlightW, height - 8, Color{180, 210, 255});
    }

    // Render Text Glyphs
    penX = startPenX;
    prevChar = '\0';
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        auto glyph = font.GetGlyph(c);
        if (glyph.width == 0 && c != ' ') continue;

        if (prevChar != '\0') {
            penX += (font.GetKerning(c, prevChar).x >> 6);
        }

        int glyphLeft = penX + glyph.bearingX;
        int glyphTop = baselineY - glyph.bearingY;

        // Optional: Change text color if character is selected inside the box
        Color currentTextColor = (hasSelection && (int)i >= selMin && (int)i < selMax)
                                 ? Color{0, 45, 120} : Color{0, 0, 0};

        renderer->DrawGlyph(glyphLeft, glyphTop, glyph, currentTextColor);

        penX += glyph.advance;
        prevChar = c;
    }

    // Render Flashing/Static Insertion Vertical Cursor line
    if (focusID == id) {
        int cursorXPos = charXPositions[cursorIndex];
        // Draw a neat, 2-pixel wide cursor bar
        renderer->FillRect(cursorXPos, y + 4, 2, height - 8, Color{20, 20, 20});
    }

    return (focusID == id && io.enterPressed);
}
// --- Utility Inner Kernels ---
UIID InterfaceManager::GetID(const std::string& str) {
    // FNV-1a Hash function for reliable compile/runtime string evaluations
    uint32_t hash = 2166136261U;
    for (char c : str) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619U;
    }
    return hash;
}

bool InterfaceManager::IsMouseOver(int x, int y, int w, int h) const {
    return (io.mouseX >= x && io.mouseX <= x + w && io.mouseY >= y && io.mouseY <= y + h);
}