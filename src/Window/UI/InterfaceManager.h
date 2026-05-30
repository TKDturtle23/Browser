#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../../Render/Renderer.h"


#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Forward declarations — same types your main project already defines
class Renderer;
class Font;
enum class Key;
struct Color;
struct Glyph;

using UIID = uint32_t;

// ---------------------------------------------------------------------------
// IOState
// ---------------------------------------------------------------------------
struct DebugIOState {
    int  mouseX          = 0;
    int  mouseY          = 0;
    bool mouseLeftDown   = false;
    bool mouseLeftClicked = false;
    int  mouseWheelDelta = 0;   // +1 scroll up, -1 scroll down (set per-frame before BeginFrame)

    char lastTypedChar    = 0;
    bool backspacePressed = false;
    bool enterPressed     = false;
};

// ---------------------------------------------------------------------------
// ClipRect  —  axis-aligned scissor region
// ---------------------------------------------------------------------------
struct ClipRect {
    int x = 0, y = 0, w = 0, h = 0;
    bool active = false;
};

// ---------------------------------------------------------------------------
// Per-widget persistent state (text fields)
// ---------------------------------------------------------------------------
struct TextFieldState {
    int  cursorIndex      = 0;
    int  selectStartIndex = -1;
    bool isDragging       = false;
};

// ---------------------------------------------------------------------------
// Per-widget persistent state (scrollable lists)
// ---------------------------------------------------------------------------
struct ListState {
    int scrollOffsetY  = 0;   // Pixel scroll offset (always >= 0)
    int selectedRow    = -1;  // -1 = nothing selected
};

// ---------------------------------------------------------------------------
// ScrollableList row descriptor  —  caller fills a vector of these
// ---------------------------------------------------------------------------
struct ListRow {
    std::string text;          // Main label drawn in the row

    int IndentLevel = 0; // allows collapsing and that stuff.
    bool isCollapsed = true;
    bool Visible = true; // for manager

    // Optional badge drawn to the left of the text (e.g. "ERR", "WARN", "LOG")
    std::string badgeLabel;
    Color       badgeColor  = {180, 180, 180};
    Color       badgeText   = {255, 255, 255};

    Color       TextColor   = {200, 202, 205};
    Color       AnnotationColor = {120, 122, 125};

    // Optional right-aligned annotation (e.g. "fetch.js:21")
    std::string annotation;

    // Row tint (0,0,0,0 = no tint; set alpha > 0 to blend)
    Color rowTint           = {0, 0, 0};
    bool  hasTint           = false;
};

// ---------------------------------------------------------------------------
// DebugInterfaceManager
// ---------------------------------------------------------------------------
class DebugInterfaceManager {
public:
    DebugInterfaceManager(int initialWidth, int initialHeight);
    ~DebugInterfaceManager() = default;

    // --- Window management --------------------------------------------------
    void Resize(int newWidth, int newHeight);
    const std::vector<Color>& GetFrontBuffer() const;

    // --- Input injection ----------------------------------------------------
    void InjectMouseMove  (int x, int y);
    void InjectMouseButton(bool leftDown);
    void InjectMouseWheel (int delta);          // Call with +1 / -1 each wheel tick
    void InjectKeyChar    (Key key, bool shiftPressed = false);

    // --- Frame lifecycle ----------------------------------------------------
    void BeginFrame();
    void EndFrame();

    // --- Layout helpers -----------------------------------------------------
    void SameLine (int spacing = 5);
    void NewLine  (int spacing = 5);
    void SetCursor(int x, int y);
    int  GetCursorX() const { return cursorX; }
    int  GetCursorY() const { return cursorY; }

    // --- Clip rect ----------------------------------------------------------
    // Push a scissor region; all draws are clipped to it until PopClipRect().
    // Calls may be nested — inner rect is intersected with the current one.
    void PushClipRect(int x, int y, int w, int h);
    void PopClipRect();

    // --- Widgets ------------------------------------------------------------

    // Plain text label. Returns the pixel width of the rendered text.
    int  Label(const std::string& text,
               Color color   = {20,  20,  20},
               int   maxWidth = 0);           // 0 = unlimited; clips/ellipsis at maxWidth

    // Horizontal rule across the full panel width
    void Separator(Color color = {180, 182, 185});

    // Standard push-button.  Returns true on click.
    bool Button(const std::string& label, int width, int height);

    // Tab strip button.  Returns true when clicked/selected.
    // title is editable via keyboard when the tab has focus (same as your original).
    bool Tab(const std::string& id_str, std::string& title,
             bool isActive, int width, int height);

    // Single-line text field with cursor, selection, keyboard input.
    // Returns true when Enter is pressed.
    bool TextField(const std::string& id_str, std::string& text,
                   int width, int height);

    // Address-bar variant (blue focus ring, same as original).
    // Returns true when Enter is pressed.
    bool AddressBar(const std::string& id_str, std::string& text,
                    int width, int height);

    // Checkbox with a label to the right.
    // checked is toggled in-place. Returns true on the frame it changes.
    bool Checkbox(const std::string& id_str, const std::string& label,
                  bool& checked);

    // Small filled pill badge (e.g. "ERR" / "200" / "GET").
    // Does NOT advance the layout cursor — call SameLine() before/after as needed.
    void ColorBadge(const std::string& label,
                    Color bg, Color fg,
                    int paddingX = 6, int paddingH = 4);

    // Scrollable, clipped, selectable list.
    //
    // id_str     — stable widget identity string
    // rows       — caller-owned row descriptors; can be rebuilt every frame
    // width/height — pixel dimensions of the list viewport
    // rowHeight  — pixel height of each individual row (default 20)
    //
    // Returns the index of the row that was clicked this frame, or -1.
    // Persistent selection is stored internally; read it via GetListSelection().
    int  ScrollableList(const std::string& id_str,
                        std::vector<ListRow>& rows,
                        int width, int height, bool flip = false,
                        int rowHeight = 20);

    // Returns the currently selected row index for a list (-1 = none).
    int  GetListSelection(const std::string& id_str);

    // Clears the selection for a list (e.g. when the log is cleared).
    void ClearListSelection(const std::string& id_str);

    // Scroll a list to the bottom programmatically (e.g. "auto-scroll" on new log).
    void ScrollListToBottom(const std::string& id_str,
                            const std::vector<ListRow>& rows,
                            int viewportHeight, int rowHeight = 20);

    // Draws a flat panel background rect and sets the cursor to its top-left
    // interior (with optional padding).  Does NOT push a clip rect — call
    // PushClipRect yourself if you want clipping.
    // Returns the inner width (width - 2*paddingX).
    int  BeginPanel(int x, int y, int width, int height,
                    Color bg      = {230, 232, 235},
                    int paddingX  = 6,
                    int paddingY  = 6);

    // Companion to BeginPanel — no-op currently but kept for symmetry and
    // future use (e.g. auto-popping clip rects).
    void EndPanel();

private:
    // --- Helpers ------------------------------------------------------------
    UIID GetID        (const std::string& str) const;
    bool IsMouseOver  (int x, int y, int w, int h) const;
    bool IsInsideClip (int x, int y, int w, int h) const;

    // Internal text rendering — respects active clip rect.
    // Returns the pen-X position after the last glyph.
    int  DrawText(const std::string& text,
                  int x, int baselineY,
                  Color color,
                  int maxWidth = 0);

    // Shared guts for TextField and AddressBar
    bool TextInputWidget(UIID id,
                         std::string& text,
                         int x, int y, int width, int height,
                         Color focusBorderColor);

    // --- Renderer / font ----------------------------------------------------
    std::unique_ptr<Renderer> renderer;
    Font                      font;

    int windowWidth;
    int windowHeight;

    // --- Layout state -------------------------------------------------------
    int cursorX      = 5;
    int cursorY      = 5;
    int maxRowHeight = 0;

    // --- IO -----------------------------------------------------------------
    DebugIOState io;
    bool         lastMouseState = false;

    // --- IMGUI ID tracking --------------------------------------------------
    UIID activeID = 0;
    UIID hotID    = 0;
    UIID focusID  = 0;

    // --- Per-widget persistent state ----------------------------------------
    std::unordered_map<UIID, TextFieldState> textFieldStates;
    std::unordered_map<UIID, ListState>      listStates;

    // --- Clip rect stack ----------------------------------------------------
    std::vector<ClipRect> clipStack;
    ClipRect              activeClip;   // The currently effective rect (intersection of stack)
};