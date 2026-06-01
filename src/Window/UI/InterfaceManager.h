#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <stack>
#include "Platform/Platform.h"   // Color, Key, Font, Renderer, etc.
#include "Text/Font.h"
#include "Node.h"
// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
class Renderer;

// ---------------------------------------------------------------------------
// Primitive types
// ---------------------------------------------------------------------------
using UIID = uint32_t;
constexpr UIID UI_ID_NONE = 0;

struct Vec2  { int x = 0, y = 0; };
enum CornerFlags : uint8_t {
    Corner_None        = 0,
    Corner_TopLeft     = 1 << 0,
    Corner_TopRight    = 1 << 1,
    Corner_BottomLeft  = 1 << 2,
    Corner_BottomRight = 1 << 3,
    Corner_All         = Corner_TopLeft | Corner_TopRight | Corner_BottomLeft | Corner_BottomRight,
    Corner_Top         = Corner_TopLeft | Corner_TopRight,
    Corner_Bottom      = Corner_BottomLeft | Corner_BottomRight,
    Corner_Left        = Corner_TopLeft | Corner_BottomLeft,
    Corner_Right       = Corner_TopRight | Corner_BottomRight,
};
// ---------------------------------------------------------------------------
// WidgetResult  (replaces scattered bool returns)
// Every interactive widget returns this.  Callers can check exactly what
// happened without relying on side-effects.
// ---------------------------------------------------------------------------
struct WidgetResult {
    bool hovered   = false;  // mouse is over the widget this frame
    bool held      = false;  // mouse button is held on it
    bool activated = false;  // primary action fired (click/enter/toggle)
    bool changed   = false;  // value changed (sliders, text, checkboxes …)

    // Convenience: treat the result as bool → true when activated
    explicit operator bool() const { return activated; }
};

// ---------------------------------------------------------------------------
// Style  (mirrors ImGuiStyle; sub-structs for each widget family)
// Use PushStyleColor / PushStyleVar to override locally.
// ---------------------------------------------------------------------------
struct UIStyleColors {
    // Window / panel
    Color WindowBg        = {240, 240, 240, 255};
    Color WindowBorder    = {180, 180, 180, 255};

    // Widgets – shared
    Color Text            = {30,  30,  30,  255};
    Color TextDisabled    = {160, 160, 160, 255};

    // Button
    Color ButtonNormal    = {220, 220, 220, 255};
    Color ButtonHover     = {200, 210, 235, 255};
    Color ButtonActive    = {170, 190, 225, 255};
    Color ButtonText      = {30,  30,  30,  255};

    // Input (TextField / AddressBar)
    Color InputBg         = {255, 255, 255, 255};
    Color InputBorderIdle = {190, 190, 190, 255};
    Color InputBorderFocus= {60, 120, 210, 255};
    Color InputText       = {20,  20,  20,  255};
    Color InputSelection  = {180, 210, 255, 200};
    Color InputSelText    = {10,  10,  10,  255};
    Color InputCursor     = {50,  50,  50,  255};
    Color InputFocused    = {250, 250, 255, 255};

    // Checkbox
    Color CheckboxIdle    = {210, 210, 210, 255};
    Color CheckboxChecked = {60, 120, 210, 255};
    Color CheckboxMark    = {255, 255, 255, 255};

    // Tab
    Color TabIdle         = {210, 210, 210, 255};
    Color TabHover        = {225, 225, 235, 255};
    Color TabActive       = {240, 240, 240, 255};
    Color TabAccent       = {60, 120, 210, 255};
    Color TabText         = {40,  40,  40,  255};

    // List
    Color ListBg          = {248, 248, 248, 255};
    Color ListRowEven     = {248, 248, 248, 255};
    Color ListRowOdd      = {240, 240, 240, 255};
    Color ListRowHover    = {210, 225, 255, 120};
    Color ListRowSelected = {60, 120, 210, 255};
    Color ListBorder      = {190, 190, 190, 255};
    Color ListScrollTrack = {225, 225, 225, 255};
    Color ListScrollThumb = {170, 170, 170, 255};
    Color ListArrow       = {100, 100, 100, 255};

    // Separator
    Color Separator       = {200, 200, 200, 255};
};

struct UIStyleVars {
    int   WindowPaddingX    = 8;
    int   WindowPaddingY    = 8;
    int   ItemSpacingX      = 5;
    int   ItemSpacingY      = 4;
    int   ButtonRounding    = 2;
    CornerFlags ButtonRoundingCorners = Corner_All;
    int   InputRounding     = 2;
    CornerFlags InputRoundingCorners  = Corner_All;
    int   TabRounding       = 3;
    CornerFlags TabRoundingCorners   = Corner_All;
    int   CheckboxRounding  = 2;
    CornerFlags CheckboxRoundingCorners = Corner_All;
    int   BadgeRounding     = 3;
    CornerFlags BadgeRoundingCorners = Corner_All;
    int   DefaultItemHeight = 22;

    int TreeNodeSpacingX  = 10;
};

struct UIStyle {
    UIStyleColors Colors;
    UIStyleVars   Vars;
};

// ---------------------------------------------------------------------------
// IO  (input state for one frame)
// ---------------------------------------------------------------------------
struct UIInputState {
    int  mouseX          = 0;
    int  mouseY          = 0;
    bool mouseLeftDown   = false;
    bool mouseLeftClicked= false;   // true only on the frame the button went down
    int  mouseWheelDelta = 0;

    char lastTypedChar   = '\0';
    bool backspacePressed= false;
    bool enterPressed    = false;
};

// ---------------------------------------------------------------------------
// Clip rect
// ---------------------------------------------------------------------------
struct ClipRect {
    int  x = 0, y = 0, w = 0, h = 0;
    bool active = false;
};

// ---------------------------------------------------------------------------
// Layout context
// ---------------------------------------------------------------------------
enum class LayoutDir { Vertical, Horizontal };

struct LayoutContext {
    LayoutDir dir        = LayoutDir::Vertical;
    int       originX    = 0;
    int       originY    = 0;
    int       cursorX    = 0;
    int       cursorY    = 0;
    int       maxRowH    = 0;   // tallest widget in current row (horizontal) / col (vertical)
    int       maxColW    = 0;
    int       contentW   = 0;   // available width hint (0 = unconstrained)
    int       contentH   = 0;

    int lastAdvanceY = 0;
    int lastAdvanceH = 0;
    int lastAdvanceW = 0;
};

// ---------------------------------------------------------------------------
// Window / panel descriptor
// ---------------------------------------------------------------------------
struct UIWindow {
    std::string id;
    Rect        rect;
    Color       bg;
    int         paddingX = 8;
    int         paddingY = 8;
    bool        hasBorder = true;
    bool        scrollable= false;
    int         scrollOffsetY = 0;
    int         contentHeight = 0; // accumulated during frame
};

// ---------------------------------------------------------------------------
// Per-widget persistent state
// ---------------------------------------------------------------------------
struct TextFieldState {
    int    cursorIndex      = 0;
    int    selectStartIndex = 0;
    bool   isDragging       = false;
    bool   cursorBlink      = true;
    double CursorBlinkTime  = 0.0;
    double CursorLastBlink  = 0.0;
};

struct ListState {
    int scrollOffsetY = 0;
    int selectedRow   = -1;
};

// ---------------------------------------------------------------------------
// ListRow  (unchanged from original)
// ---------------------------------------------------------------------------
struct ListRow {
    std::string text;
    std::string annotation;
    std::string badgeLabel;
    bool        hasTint          = false;
    bool        isCollapsed      = false;
    int         IndentLevel      = 0;
};
struct UI_Image {
    std::string svg;
    int         width = 0;
    int         height = 0;
    std::vector<Color> img;

    UI_Image() = default;

};

// ---------------------------------------------------------------------------
// Style-push entries (for PushStyleColor / PushStyleVar)
// ---------------------------------------------------------------------------
enum class UIColorVar {
    WindowBg, WindowBorder,
    Text, TextDisabled,
    ButtonNormal, ButtonHover, ButtonActive, ButtonText,
    InputBg, InputBorderIdle, InputBorderFocus, InputText,
    InputSelection, InputSelText, InputCursor,
    CheckboxIdle, CheckboxChecked, CheckboxMark,
    TabIdle, TabHover, TabActive, TabAccent, TabText,
    ListBg, ListRowEven, ListRowOdd, ListRowHover, ListRowSelected,
    ListBorder, ListScrollTrack, ListScrollThumb, ListArrow,
    Separator,
    COUNT
};

// ---------------------------------------------------------------------------
//  UIManager  —  the main class
// ---------------------------------------------------------------------------
class UIManager {
public:
    // -----------------------------------------------------------------------
    // Construction / resize
    // -----------------------------------------------------------------------
    UIManager(int initialWidth, int initialHeight);
    ~UIManager();
    void Resize(int newWidth, int newHeight);
    Renderer *GetRenderer() const {return renderer.get();}

    const std::vector<Color>& GetFrontBuffer() const;
    void SetRowHeight(int h);

    UI_Image MakeImage(const std::string& svg, int width, int height);

    // -----------------------------------------------------------------------
    // Style
    // -----------------------------------------------------------------------
    UIStyle*       GetStyle()       { return &style; }
    const UIStyle* GetStyle() const { return &style; }

    void PushStyleColor(UIColorVar var, Color color);
    void PopStyleColor(int count = 1);


    // -----------------------------------------------------------------------
    // Input injection  (call before BeginFrame)
    // -----------------------------------------------------------------------
    void InjectMouseMove  (int x, int y);
    void InjectMouseButton(bool leftDown);
    void InjectMouseWheel (int delta);
    void InjectKeyChar    (Key key, bool shiftPressed);

    // -----------------------------------------------------------------------
    // Frame lifecycle
    // -----------------------------------------------------------------------
    void BeginFrame();
    void EndFrame();

    bool RedrawRequested() const { return redrawNeeded; }
    void RequestRedraw()         { redrawNeeded = true; }

    // -----------------------------------------------------------------------
    // ID stack  (scope widget IDs within loops / reusable components)
    //   PushID("item_42");  Button("delete");  PopID();
    // -----------------------------------------------------------------------
    void        PushID(const std::string& id);
    void        PushID(int id);
    void        PopID();

    // -----------------------------------------------------------------------
    // Layout  — cursor, grouping, horizontal rows
    // -----------------------------------------------------------------------
    void SameLine (int spacing = -1);  // -1 → use ItemSpacingX
    void NewLine  (int spacing = -1);
    void SetCursor(int x, int y);

    Vec2 GetCursor() const;

    // Group: capture a region; useful for sizing or custom hit-testing
    void BeginGroup();
    Rect EndGroup();       // returns bounding rect of everything drawn inside

    // Horizontal layout: widgets placed left-to-right until EndHorizontal()
    void BeginHorizontal(int spacing = -1);
    void EndHorizontal();

    // Indent / Unindent
    void Indent  (int width = 16);
    void Unindent(int width = 16);

    // -----------------------------------------------------------------------
    // Window / panel stack
    //   Returns usable content width (rect.w - 2*paddingX).
    //   scrollable=true enables a vertical scrollbar automatically.
    // -----------------------------------------------------------------------
    int  BeginWindow(const std::string& id, Rect rect,
                     Color bg = {240, 240, 240,255},
                     int paddingX = 8, int paddingY = 8,
                     bool hasBorder = true, bool scrollable = false);
    void EndWindow();
    void RowBackground(int height, Color color);
    // Scroll-area without the window chrome (background/border).
    // Place inside an existing window; clip+scroll a region manually.
    void BeginScrollArea(const std::string& id, int width, int height);
    void EndScrollArea();

    // -----------------------------------------------------------------------
    // Clip rect stack  (exposed so custom draw calls can respect it)
    // -----------------------------------------------------------------------
    void PushClipRect(int x, int y, int w, int h);
    void PopClipRect();
    ClipRect GetClipRect() const { return activeClip; }

    // -----------------------------------------------------------------------
    // Widgets  — all return WidgetResult
    // -----------------------------------------------------------------------

    // Static
    WidgetResult Label    (const std::string& text,
                           Color color = {220,220,220,255}, int maxWidth = 0);
    void         Separator(Color color = {60,60,60,255});

    // Interactive
    WidgetResult Button  (const std::string& label,
                          int width = 80, int height = -1);
    WidgetResult SvgButton(const std::string& id, const UI_Image& svg,
                          int width = 80, int height = -1);

    WidgetResult Tab     (const std::string& id_str, std::string& title,
                          bool isActive, int width = 100, int height = -1);
    WidgetResult Checkbox(const std::string& id_str,
                          const std::string& label, bool& checked);

    // Color badge (decorative, non-interactive — returns WidgetResult with hovered)
    WidgetResult ColorBadge(const std::string& label,
                            Color bg = {70,120,200,255},
                            Color fg = {255,255,255,255},
                            int paddingX = 4, int paddingH = 2);

    // Text input
    WidgetResult TextField  (const std::string& id_str, std::string& text,
                             int width = 200, int height = -1);
    WidgetResult AddressBar (const std::string& id_str, std::string& text,
                             int width = 200, int height = -1);
    void Text(const std::string &id_str, const std::string &text,
              int width = -1, int height = -1);
    // Scrollable list — returns clicked row index (≥0) or -1 via result.activated
    // result.activated == true  →  a row was clicked; check GetListSelection()
    bool BeginListBox(const std::string& id_str, int width, int height,
                  bool flip = false, int rowHeight = 20);
    void EndListBox();
    bool Selectable(const std::string& id_str, const std::string& text, bool selected,
                int width = 0, int height = -1,
                Color textColor = {0, 0, 0, 0});

    int  GetListSelection  (const std::string& id_str);
    void ClearListSelection(const std::string& id_str);
    void ScrollListToBottom(const std::string& id_str,
                            int rows,
                            int viewportHeight, int rowHeight = 20);

    bool TreeNode(const std::string& id_str, const std::string& label, bool &clicked, bool Selected, int width = 0, int height = -1);
    void TreePop(const std::string &id_str);
    bool IsTreeOpen(const std::string& id_str) const;
    WidgetResult ProgressBar(float t, int width, int height);

    // -----------------------------------------------------------------------
    // Low-level draw calls  (for custom widgets / overlays)
    // All coordinates are absolute screen pixels.
    // -----------------------------------------------------------------------
    void DrawRect     (int x, int y, int w, int h, Color c);
    void FillRect     (int x, int y, int w, int h, Color c);

    void DrawText     (const std::string& text,
                       int x, int baselineY,
                       Color color, int maxWidth = 0);
    int  MeasureText  (const std::string& text) const;
    void FillRectRound(int x, int y, int w, int h, int r, Color c, uint8_t corners) const;
    // -----------------------------------------------------------------------
    // Misc
    // -----------------------------------------------------------------------
    bool IsMouseOverAnyWidget() const { return mouseOverAnyWidget; }
    Vec2 GetMousePos()          const { return {io.mouseX, io.mouseY}; }
    bool IsMouseDown()          const { return io.mouseLeftDown; }
    bool IsMouseClicked()       const { return io.mouseLeftClicked; }
    bool IsMouseOver(int x, int y, int w, int h) const;

    int  WindowWidth()  const { return windowWidth; }
    int  WindowHeight() const { return windowHeight; }
    void AdvanceCursorX(int x);
    void AdvanceCursorY(int y);

private:
    // -----------------------------------------------------------------------
    // Internal
    // -----------------------------------------------------------------------
    UIID GetID(const std::string& str) const;
    UIID GetScopedID(const std::string& str) const; // respects ID stack

    bool IsInsideClip(int x, int y, int w, int h) const;
    int  DrawTextInternal(const std::string& text,
                          int x, int baselineY,
                          Color color, int maxWidth);

    // Shared core of TextField / AddressBar
    WidgetResult TextInputWidget(UIID id,
                                 std::string& text,
                                 int x, int y, int w, int h);

    // Advance the layout cursor after placing a widget of (w×h)
    void AdvanceCursor(int w, int h);

    // Retrieve / lazily create persistent state
    TextFieldState& GetTextFieldState(UIID id) { return textFieldStates[id]; }
    ListState&      GetListState     (UIID id) { return listStates[id]; }

    Color* StyleColor(UIColorVar var);   // pointer into active style

    // -----------------------------------------------------------------------
    // Data
    // -----------------------------------------------------------------------
    int  windowWidth  = 0;
    int  windowHeight = 0;
    bool redrawNeeded = false;

    std::unique_ptr<Renderer> renderer;
    Font                      font;
    UIStyle                   style;

    UIInputState io;
    bool         lastMouseState = false;

    UIID hotID    = UI_ID_NONE;
    UIID activeID = UI_ID_NONE;
    UIID focusID  = UI_ID_NONE;

    bool mouseOverAnyWidget = false;

    // Layout stack
    std::vector<LayoutContext> layoutStack;
    LayoutContext& Layout() { return layoutStack.back(); }

    // Window stack
    std::vector<UIWindow> windowStack;

    // Clip stack
    std::vector<ClipRect> clipStack;
    ClipRect              activeClip;

    // ID stack
    std::vector<std::string> idStack;

    // Style override stacks
    struct StyleColorEntry { UIColorVar var; Color previous; };
    std::vector<StyleColorEntry> styleColorStack;

    // Persistent per-widget state
    std::unordered_map<UIID, TextFieldState> textFieldStates;
    std::unordered_map<UIID, ListState>      listStates;

    // Group tracking
    struct GroupState { Rect bounds; int layoutIdx; };
    std::vector<GroupState> groupStack;

    // Scroll-area stack
    struct ScrollArea {
        UIID id;
        int  x, y, w, h;
        int  contentStartY;
    };
    std::vector<ScrollArea> scrollAreaStack;
    std::unordered_map<UIID, int> scrollAreaOffsets; // persistent Y offsets

    struct ListBoxState { // scrollable lists
        int  scrollOffsetY = 0;
        int  selectedRow   = -1;
        int  hoveredRow    = -1;
        int contentHeight = 0;
    };
    std::unordered_map<UIID, ListBoxState> listBoxStates;

    // Active listbox context (set between Begin/End)
    struct ListBoxContext {
        UIID  id;
        int   x, y, w, h;
        int   rowHeight;
        int   visibleRow;   // incremented by each Selectable call
        bool  flip;
        ListBoxState* state = nullptr;
    };
    ListBoxContext activeListBox;
    bool          inListBox = false;

    struct TreeNodeState {
        UIID id;
        int  indentLevel;
        bool expanded = false;
    };
    std::unordered_map<UIID, TreeNodeState> treeNodeStates;
};