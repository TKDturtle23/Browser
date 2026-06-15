#include "InterfaceManager.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <numeric>
#include <sstream>

#include "Render/Renderer.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {
    // FNV-1a hash of an arbitrary string
    constexpr uint32_t fnv1a(const char* s, uint32_t hash = 2166136261U) {
        return (*s == '\0') ? hash : fnv1a(s + 1, (hash ^ (uint8_t)*s) * 16777619U);
    }
    uint32_t fnv1a(const std::string& s) {
        uint32_t h = 2166136261U;
        for (char c : s) { h ^= (uint8_t)c; h *= 16777619U; }
        return h;
    }

    double NowMs() {
        using namespace std::chrono;
        return (double)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
}

// ---------------------------------------------------------------------------
// Construction / resize
// ---------------------------------------------------------------------------

UIManager::UIManager(int w, int h)
    : windowWidth(w), windowHeight(h),
      font("Fonts/arial/ARIAL.TTF", 14)
{
    renderer = std::make_unique<RendererSurface>(w, h);

    // Push a root layout context
    LayoutContext root;
    root.originX = root.cursorX = 0;
    root.originY = root.cursorY = 0;
    root.contentW = w;
    root.contentH = h;
    layoutStack.push_back(root);
}
UIManager::~UIManager() = default;
void UIManager::Resize(int w, int h) {
    windowWidth  = w;
    windowHeight = h;
    renderer->Resize(w, h);
    if (!layoutStack.empty()) {
        layoutStack.front().contentW = w;
        layoutStack.front().contentH = h;
    }
}

// ---------------------------------------------------------------------------
// Style  — push / pop color overrides
// ---------------------------------------------------------------------------

Color* UIManager::StyleColor(UIColorVar var) {
    auto& c = style.Colors;
    switch (var) {
        case UIColorVar::WindowBg:         return &c.WindowBg;
        case UIColorVar::WindowBorder:     return &c.WindowBorder;
        case UIColorVar::Text:             return &c.Text;
        case UIColorVar::TextDisabled:     return &c.TextDisabled;
        case UIColorVar::ButtonNormal:     return &c.ButtonNormal;
        case UIColorVar::ButtonHover:      return &c.ButtonHover;
        case UIColorVar::ButtonActive:     return &c.ButtonActive;
        case UIColorVar::ButtonText:       return &c.ButtonText;
        case UIColorVar::InputBg:          return &c.InputBg;
        case UIColorVar::InputBorderIdle:  return &c.InputBorderIdle;
        case UIColorVar::InputBorderFocus: return &c.InputBorderFocus;
        case UIColorVar::InputText:        return &c.InputText;
        case UIColorVar::InputSelection:   return &c.InputSelection;
        case UIColorVar::InputSelText:     return &c.InputSelText;
        case UIColorVar::InputCursor:      return &c.InputCursor;
        case UIColorVar::CheckboxIdle:     return &c.CheckboxIdle;
        case UIColorVar::CheckboxChecked:  return &c.CheckboxChecked;
        case UIColorVar::CheckboxMark:     return &c.CheckboxMark;
        case UIColorVar::TabIdle:          return &c.TabIdle;
        case UIColorVar::TabHover:         return &c.TabHover;
        case UIColorVar::TabActive:        return &c.TabActive;
        case UIColorVar::TabAccent:        return &c.TabAccent;
        case UIColorVar::TabText:          return &c.TabText;
        case UIColorVar::ListBg:           return &c.ListBg;
        case UIColorVar::ListRowEven:      return &c.ListRowEven;
        case UIColorVar::ListRowOdd:       return &c.ListRowOdd;
        case UIColorVar::ListRowHover:     return &c.ListRowHover;
        case UIColorVar::ListRowSelected:  return &c.ListRowSelected;
        case UIColorVar::ListBorder:       return &c.ListBorder;
        case UIColorVar::ListScrollTrack:  return &c.ListScrollTrack;
        case UIColorVar::ListScrollThumb:  return &c.ListScrollThumb;
        case UIColorVar::ListArrow:        return &c.ListArrow;
        case UIColorVar::Separator:        return &c.Separator;
        default:                           return nullptr;
    }
}

void UIManager::PushStyleColor(UIColorVar var, Color color) {
    Color* ptr = StyleColor(var);
    if (!ptr) return;
    styleColorStack.push_back({var, *ptr});
    *ptr = color;
}

void UIManager::PopStyleColor(int count) {
    for (int i = 0; i < count && !styleColorStack.empty(); ++i) {
        auto& e = styleColorStack.back();
        Color* ptr = StyleColor(e.var);
        if (ptr) *ptr = e.previous;
        styleColorStack.pop_back();
    }
}

void UIManager::FillRectRound(int x, int y, int w, int h, int r, Color c, uint8_t corners) const {
    renderer->FillRectBeveled(x, y, w, h, r, c); // bevel all corners first

    // Overdraw any corners that should be square
    if (!(corners & Corner_TopLeft))     renderer->FillRect(x,         y,         r, r, c);
    if (!(corners & Corner_TopRight))    renderer->FillRect(x + w - r, y,         r, r, c);
    if (!(corners & Corner_BottomLeft))  renderer->FillRect(x,         y + h - r, r, r, c);
    if (!(corners & Corner_BottomRight)) renderer->FillRect(x + w - r, y + h - r, r, r, c);
}

// ---------------------------------------------------------------------------
// Input injection
// ---------------------------------------------------------------------------

void UIManager::InjectMouseMove(int x, int y) {
    io.mouseX = x; io.mouseY = y;
}
void UIManager::InjectMouseButton(bool leftDown) {
    io.mouseLeftDown = leftDown;
}
void UIManager::InjectMouseWheel(int delta) {
    io.mouseWheelDelta = delta;
}

void UIManager::InjectKeyChar(Key key, bool shift) {
    io.backspacePressed = (key == Key::Backspace);
    io.enterPressed     = (key == Key::Return || key == Key::NumpadEnter);

    char c = '\0';
    if (key >= Key::A && key <= Key::Z) {
        int off = static_cast<int>(key) - static_cast<int>(Key::A);
        c = shift ? ('A' + off) : ('a' + off);
    } else if (key >= Key::Num0 && key <= Key::Num9) {
        int off = static_cast<int>(key) - static_cast<int>(Key::Num0);
        if (shift) {
            const char shiftNums[] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
            c = shiftNums[off];
        } else {
            c = '0' + off;
        }
    } else if (key >= Key::Numpad0 && key <= Key::Numpad9) {
        c = '0' + (static_cast<int>(key) - static_cast<int>(Key::Numpad0));
    } else {
        switch (key) {
            case Key::Space:          c = ' ';                       break;
            case Key::NumpadDivide:   c = '/';                       break;
            case Key::NumpadMultiply: c = '*';                       break;
            case Key::NumpadSubtract: c = '-';                       break;
            case Key::NumpadAdd:      c = '+';                       break;
            case Key::NumpadDecimal:  c = '.';                       break;
            case Key::Semicolon:      c = shift ? ':' : ';';         break;
            case Key::Slash:          c = shift ? '?' : '/';         break;
            case Key::Equal:          c = shift ? '+' : '=';         break;
            case Key::Hyphen:         c = shift ? '_' : '-';         break;
            case Key::LBracket:       c = shift ? '{' : '[';         break;
            case Key::RBracket:       c = shift ? '}' : ']';         break;
            case Key::Comma:          c = shift ? '<' : ',';         break;
            case Key::Period:         c = shift ? '>' : '.';         break;
            case Key::Quote:          c = shift ? '"' : '\'';        break;
            case Key::Backquote:      c = shift ? '~' : '`';         break;
            case Key::Backslash:      c = shift ? '|' : '\\';        break;
            default:                  c = '\0';                      break;
        }
    }
    io.lastTypedChar = c;
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------

void UIManager::BeginFrame() {
    io.mouseLeftClicked = (io.mouseLeftDown && !lastMouseState);
    lastMouseState      = io.mouseLeftDown;

    renderer->Clear(style.Colors.WindowBg);

    hotID              = UI_ID_NONE;
    mouseOverAnyWidget = false;

    // Reset root layout cursor
    auto& root = layoutStack.front();
    root.cursorX = root.originX;
    root.cursorY = root.originY;
    root.maxRowH = 0;
    root.maxColW = 0;
}

void UIManager::EndFrame() {
    if (!io.mouseLeftDown) activeID = UI_ID_NONE;

    io.lastTypedChar    = '\0';
    io.backspacePressed = false;
    io.enterPressed     = false;
    io.mouseWheelDelta  = 0;
    redrawNeeded        = false;

}

// ---------------------------------------------------------------------------
// ID stack
// ---------------------------------------------------------------------------

void UIManager::PushID(const std::string& id) { idStack.push_back(id); }
void UIManager::PushID(int id)                 { idStack.push_back(std::to_string(id)); }
void UIManager::PopID()  { if (!idStack.empty()) idStack.pop_back(); }

void UIManager::AdvanceCursorX(const int x) {
    auto& lc = Layout();
    lc.cursorX += x;
    if (lc.cursorX > lc.contentW) lc.cursorX = lc.contentW;
}

void UIManager::AdvanceCursorY(int y) {
    auto& lc = Layout();
    lc.cursorY += y;
    if (lc.cursorY > lc.contentH) lc.cursorY = lc.contentH;
}

UIID UIManager::GetID(const std::string& str) const {
    return fnv1a(str);
}

UIID UIManager::GetScopedID(const std::string& str) const {
    // Combine current ID stack into a single seed string
    std::string seed;
    for (auto& s : idStack) { seed += s; seed += '/'; }
    seed += str;
    return fnv1a(seed);
}

// ---------------------------------------------------------------------------
// Clip rect stack
// ---------------------------------------------------------------------------

void UIManager::PushClipRect(int x, int y, int w, int h) {
    ClipRect incoming{x, y, w, h, true};
    if (!clipStack.empty() && clipStack.back().active) {
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

void UIManager::PopClipRect() {
    if (!clipStack.empty()) clipStack.pop_back();
    activeClip = clipStack.empty() ? ClipRect{} : clipStack.back();
}

bool UIManager::IsInsideClip(int x, int y, int w, int h) const {
    if (!activeClip.active) return true;
    return !(x + w <= activeClip.x || y + h <= activeClip.y ||
             x     >= activeClip.x + activeClip.w ||
             y     >= activeClip.y + activeClip.h);
}

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

void UIManager::AdvanceCursor(int w, int h) {
    auto& lc = Layout();
    if (lc.dir == LayoutDir::Horizontal) {
        lc.cursorX += w + style.Vars.ItemSpacingX;
        if (h > lc.maxRowH) lc.maxRowH = h;
    } else {
        lc.lastAdvanceY = lc.cursorY;
        lc.lastAdvanceH = h;
        lc.lastAdvanceW = w;
        lc.cursorY += h + style.Vars.ItemSpacingY;
        lc.cursorX  = lc.originX;
        if (w > lc.maxColW) lc.maxColW = w;
    }
}

void UIManager::SameLine(int spacing) {
    auto& lc = Layout();
    if (spacing < 0) spacing = style.Vars.ItemSpacingX;

    if (lc.dir == LayoutDir::Vertical) {
        lc.cursorY  = lc.lastAdvanceY;
        lc.cursorX  = lc.originX + lc.lastAdvanceW + spacing;
        if (lc.lastAdvanceH > lc.maxRowH) lc.maxRowH = lc.lastAdvanceH;
    } else {
        lc.cursorX += spacing;
    }

    lc.dir = LayoutDir::Horizontal;
}

void UIManager::NewLine(int spacing) {
    auto& lc = Layout();
    if (spacing < 0) spacing = style.Vars.ItemSpacingY;
    lc.cursorX  = lc.originX;
    lc.cursorY += lc.maxRowH + spacing;
    lc.maxRowH  = 0;
    lc.dir      = LayoutDir::Vertical;
}

void UIManager::SetCursor(int x, int y) {
    auto& lc = Layout();
    lc.cursorX = x;
    lc.cursorY = y;
    lc.dir     = LayoutDir::Horizontal;  // add this
    // don't reset maxRowH here — let it accumulate
}
// .cpp
void UIManager::SetRowHeight(int h) {
    if (h > Layout().maxRowH) Layout().maxRowH = h;
}
#include "Images/SvgViewer.h"
UI_Image UIManager::MakeImage(const std::string &svg, int width, int height) {
    // Load img
std::ifstream in(svg, std::ios::in | std::ios::binary | std::ios::ate);
if (!in) return UI_Image{};

std::streamsize size = in.tellg();
in.seekg(0, std::ios::beg);

std::string content;
content.resize(size);

if (!in.read(&content[0], size)) {
    return UI_Image{};
}
    std::vector<uint8_t> data(content.begin(), content.end());
    // make img
if (SvgViewer::IsSvg(data)) {
SvgViewer viewer;
    int channels; // always 4
    auto raw = viewer.GetPixels(data, width, height, channels);
    UI_Image img;
    img.width = width;
    img.height = height;
    img.svg = svg;
    for (size_t i = 0; i < raw.size(); i += 4) {
        if (i + 3 < raw.size()) {
            Color pixel;

                pixel.r = raw[i];     // Byte 0
                pixel.g = raw[i + 1]; // Byte 1
                pixel.b = raw[i + 2]; // Byte 2
                pixel.a = raw[i + 3]; // Byte 3

            img.img.push_back(pixel);
        }
    }
    return img;

}
return UI_Image{};
}

Vec2 UIManager::GetCursor() const {
    return {layoutStack.back().cursorX, layoutStack.back().cursorY};
}

void UIManager::Indent(int width) {
    Layout().originX  += width;
    Layout().cursorX  += width;
}

void UIManager::Unindent(int width) {
    Layout().originX  -= width;
    Layout().cursorX  -= width;
}

// ---------------------------------------------------------------------------
// Group
// ---------------------------------------------------------------------------

void UIManager::BeginGroup() {
    auto& lc = Layout();
    groupStack.push_back({{(float)lc.cursorX, (float)lc.cursorY, 0, 0}, (int)layoutStack.size()});
}

Rect UIManager::EndGroup() {
    if (groupStack.empty()) return {};
    auto gs = groupStack.back();
    groupStack.pop_back();
    auto& lc = Layout();
    Rect r;
    r.x = gs.bounds.x;
    r.y = gs.bounds.y;
    r.width = lc.cursorX - gs.bounds.x;
    r.height = lc.cursorY - gs.bounds.y + lc.maxRowH;
    return r;
}

// ---------------------------------------------------------------------------
// Horizontal layout scope
// ---------------------------------------------------------------------------

void UIManager::BeginHorizontal(int spacing) {
    // Push a copy of the current layout context, set to horizontal
    LayoutContext lc = Layout();
    lc.dir = LayoutDir::Horizontal;
    if (spacing >= 0) lc.originX = lc.cursorX; // respect custom spacing
    layoutStack.push_back(lc);
}

void UIManager::EndHorizontal() {
    if (layoutStack.size() <= 1) return;
    // Merge tallest row height back into parent
    int rowH = layoutStack.back().maxRowH;
    int endX  = layoutStack.back().cursorX;
    layoutStack.pop_back();
    auto& parent = Layout();
    if (rowH > parent.maxRowH) parent.maxRowH = rowH;
    parent.cursorX = endX;
    parent.cursorY += rowH + style.Vars.ItemSpacingY;
    parent.maxRowH = 0;
}

// ---------------------------------------------------------------------------
// Window / panel
// ---------------------------------------------------------------------------

int UIManager::BeginWindow(const std::string& id, Rect rect,
                           Color bg, int paddingX, int paddingY,
                           bool hasBorder, bool scrollable)
{
    UIWindow win;
    win.id        = id;
    win.rect      = rect;
    win.bg        = bg;
    win.paddingX  = paddingX;
    win.paddingY  = paddingY;
    win.hasBorder = hasBorder;
    win.scrollable= scrollable;

    renderer->FillRect(rect.x, rect.y, rect.width, rect.height, bg);
    if (hasBorder)
        renderer->DrawRect(rect.x, rect.y, rect.width, rect.height, style.Colors.WindowBorder);

    PushClipRect(rect.x + paddingX, rect.y + paddingY,
                 rect.width - paddingX * 2, rect.height - paddingY * 2);

    // Push a fresh layout context for this window
    LayoutContext lc;
    lc.originX = lc.cursorX = rect.x + paddingX;
    lc.originY = lc.cursorY = rect.y + paddingY;
    lc.contentW = rect.width - paddingX * 2;
    lc.contentH = rect.height - paddingY * 2;
    layoutStack.push_back(lc);

    windowStack.push_back(win);
    return lc.contentW;
}

void UIManager::EndWindow() {
    if (windowStack.empty()) return;
    windowStack.pop_back();
    layoutStack.pop_back();
    PopClipRect();
}

void UIManager::RowBackground(int height, Color color) {
    auto& lc = Layout();
    renderer->FillRect(0, lc.cursorY, windowWidth, height, color);
}

// ---------------------------------------------------------------------------
// Scroll area  (lightweight, no chrome)
// ---------------------------------------------------------------------------

void UIManager::BeginScrollArea(const std::string& id, int width, int height) {
    UIID uid = GetScopedID(id);
    auto& lc = Layout();
    int x = lc.cursorX, y = lc.cursorY;

    int& scrollY = scrollAreaOffsets[uid];

    // Mouse-wheel handling
    if (IsMouseOver(x, y, width, height) && io.mouseWheelDelta != 0) {
        mouseOverAnyWidget = true;
        scrollY -= io.mouseWheelDelta * 20;
        if (scrollY < 0) scrollY = 0;
    }

    PushClipRect(x, y, width, height);

    ScrollArea sa;
    sa.id           = uid;
    sa.x = x; sa.y = y; sa.w = width; sa.h = height;
    sa.contentStartY= y;
    scrollAreaStack.push_back(sa);

    // Push layout offset by scrollY
    LayoutContext inner;
    inner.originX = inner.cursorX = x;
    inner.originY = inner.cursorY = y - scrollY;
    inner.contentW = width;
    inner.contentH = 0; // will grow
    layoutStack.push_back(inner);
}

void UIManager::EndScrollArea() {
    if (scrollAreaStack.empty()) return;
    ScrollArea sa = scrollAreaStack.back();
    scrollAreaStack.pop_back();

    int contentH = Layout().cursorY - (sa.y - scrollAreaOffsets[sa.id]);
    layoutStack.pop_back();
    PopClipRect();

    // Clamp scroll
    int maxScroll = std::max(0, contentH - sa.h);
    int& scrollY  = scrollAreaOffsets[sa.id];
    scrollY       = std::clamp(scrollY, 0, maxScroll);

    // Draw scrollbar if needed
    if (contentH > sa.h) {
        const int sbW = 5;
        float ratio = (float)sa.h / (float)contentH;
        int   sbH   = std::max(20, (int)(sa.h * ratio));
        float t     = (maxScroll > 0) ? (float)scrollY / (float)maxScroll : 0.f;
        int   sbY   = sa.y + (int)((sa.h - sbH) * t);
        renderer->FillRect(sa.x + sa.w - sbW, sa.y, sbW, sa.h, style.Colors.ListScrollTrack);
        renderer->FillRect(sa.x + sa.w - sbW, sbY,  sbW, sbH, style.Colors.ListScrollThumb);
    }

    // Advance parent cursor
    AdvanceCursor(sa.w, sa.h);
}

// ---------------------------------------------------------------------------
// Internal low-level draw
// ---------------------------------------------------------------------------

bool UIManager::IsMouseOver(int x, int y, int w, int h) const {
    return (io.mouseX >= x && io.mouseX < x + w &&
            io.mouseY >= y && io.mouseY < y + h);
}

void UIManager::DrawRect(int x, int y, int w, int h, Color c) {
    renderer->DrawRect(x, y, w, h, c);
}

void UIManager::FillRect(int x, int y, int w, int h, Color c) {
    renderer->FillRect(x, y, w, h, c);
}



int UIManager::DrawTextInternal(const std::string& text,
                                int x, int baselineY,
                                Color color, int maxWidth)
{
    int penX = x;
    char prev = '\0';

    bool needEllipsis = false;
    int  ellipsisW    = 0;

    if (maxWidth > 0) {
        auto eg = font.GetGlyph(IRenderBackend::GetRenderBackend().get(), '.');
        ellipsisW = eg.advance * 3;
        int totalW = 0;
        for (char c : text) totalW += font.GetGlyph(IRenderBackend::GetRenderBackend().get(), c).advance;
        needEllipsis = (totalW > maxWidth);
    }

    int avail  = needEllipsis ? (maxWidth - ellipsisW) : maxWidth;
    int drawn  = 0;

    for (char c : text) {
        auto g = font.GetGlyph(IRenderBackend::GetRenderBackend().get(), c);
        if (prev != '\0') { int k = font.GetKerning(c, prev).x >> 6; penX += k; drawn += k; }
        if (maxWidth > 0 && drawn + g.advance > avail) break;
        int gx = penX + g.bearingX, gy = baselineY - g.bearingY;
        if (IsInsideClip(gx, gy, g.width, g.height))
            renderer->DrawGlyph(gx, gy, g, color);
        penX  += g.advance;
        drawn += g.advance;
        prev   = c;
    }

    if (needEllipsis) {
        for (int i = 0; i < 3; ++i) {
            auto g = font.GetGlyph(IRenderBackend::GetRenderBackend().get(), '.');
            int gx = penX + g.bearingX, gy = baselineY - g.bearingY;
            if (IsInsideClip(gx, gy, g.width, g.height))
                renderer->DrawGlyph(gx, gy, g, color);
            penX += g.advance;
        }
    }
    return penX;
}

void UIManager::DrawText(const std::string& text, int x, int baselineY,
                         Color color, int maxWidth) {
    DrawTextInternal(text, x, baselineY, color, maxWidth);
}

int UIManager::MeasureText(const std::string& text) const {
    int w = 0;
    for (char c : text) w += font.GetGlyph(IRenderBackend::GetRenderBackend().get(), c).advance;
    return w;
}

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------

WidgetResult UIManager::Label(const std::string& text, Color color, int maxWidth) {
    auto& lc = Layout();
    int x = lc.cursorX, y = lc.cursorY;
    int baseline = y + 14;
    int endX = DrawTextInternal(text, x, baseline, color, maxWidth);
    int w = endX - x;
    int h = 16;
    AdvanceCursor(w, h);
    WidgetResult r;
    r.hovered = IsMouseOver(x, y, w, h);
    return r;
}

// ---------------------------------------------------------------------------
// Separator
// ---------------------------------------------------------------------------

void UIManager::Separator(Color color) {
    NewLine(2);
    auto& lc = Layout();
    renderer->FillRect(lc.originX, lc.cursorY, lc.contentW > 0 ? lc.contentW : windowWidth, 1, color);
    lc.cursorY  += 4;
    lc.maxRowH   = 0;
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------

WidgetResult UIManager::Button(const std::string& label, int width, int height) {
    if (height < 0) height = style.Vars.DefaultItemHeight;

    UIID id = GetScopedID(label);
    auto& lc = Layout();
    int x = lc.cursorX, y = lc.cursorY;
    AdvanceCursor(width, height);

    WidgetResult r;
    r.hovered = IsMouseOver(x, y, width, height);
    if (r.hovered) {
        mouseOverAnyWidget = true;
        hotID = id;
        if (activeID == UI_ID_NONE && io.mouseLeftClicked) activeID = id;
    }
    r.held      = (hotID == id && activeID == id);
    r.activated = (hotID == id && activeID == id && !io.mouseLeftDown);

    Color bg = style.Colors.ButtonNormal;
    if (hotID == id) bg = r.held ? style.Colors.ButtonActive : style.Colors.ButtonHover;

    FillRectRound(x, y, width, height, style.Vars.ButtonRounding, bg, style.Vars.ButtonRoundingCorners);
    DrawTextInternal(label, x + 8, y + (height / 2) + 5, style.Colors.ButtonText, width - 8);

    return r;
}

WidgetResult UIManager::SvgButton(const std::string &id, const UI_Image& svg, int width, int height) {
    if (height < 0) height = style.Vars.DefaultItemHeight;

    UIID uid = GetScopedID(id);
    auto& lc = Layout();
    int x = lc.cursorX, y = lc.cursorY;
    AdvanceCursor(width, height);

    WidgetResult r;
    r.hovered = IsMouseOver(x, y, width, height);
    if (r.hovered) {
        mouseOverAnyWidget = true;
        hotID = uid;
        if (activeID == UI_ID_NONE && io.mouseLeftClicked) activeID = uid;
    }
    r.held      = (hotID == uid && activeID == uid);
    r.activated = (hotID == uid && activeID == uid && !io.mouseLeftDown);

    Color bg = style.Colors.ButtonNormal;
    if (hotID == uid) bg = r.held ? style.Colors.ButtonActive : style.Colors.ButtonHover;

    FillRectRound(x, y, width, height, style.Vars.ButtonRounding, bg, style.Vars.ButtonRoundingCorners);
    x += 4;
    y += 4;
    int w = std::min(width - 8, svg.width);
    int h = std::min(height - 8, svg.height);
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            renderer->DrawPixel(
                x + j,
                y + i,
                svg.img[j + i * svg.width]
            );
        }
    }

    return r;
}

// ---------------------------------------------------------------------------
// Tab
// ---------------------------------------------------------------------------

WidgetResult UIManager::Tab(const std::string& id_str, std::string& title,
                            bool isActive, int width, int height)
{
    if (height < 0) height = style.Vars.DefaultItemHeight;
    UIID id = GetScopedID(id_str);
    auto& lc = Layout();
    int x = lc.cursorX, y = lc.cursorY;
    AdvanceCursor(width, height);

    WidgetResult r;
    r.hovered = IsMouseOver(x, y, width, height);
    if (r.hovered) {
        mouseOverAnyWidget = true;
        hotID = id;
        if (io.mouseLeftClicked) {
            activeID = id; focusID = id;
            r.activated = true;
        }
    } else if (io.mouseLeftClicked && focusID == id && !isActive) {
        focusID = UI_ID_NONE;
    }

    if (focusID == id) {
        if (io.backspacePressed && !title.empty()) { title.pop_back(); r.changed = true; }
        if (io.lastTypedChar >= 32 && io.lastTypedChar <= 126) {
            title.push_back(io.lastTypedChar); r.changed = true;
        }
        if (io.enterPressed) r.activated = true;
    }

    Color bg = isActive ? style.Colors.TabActive : style.Colors.TabIdle;
    if (!isActive && hotID == id) bg = style.Colors.TabHover;

    FillRectRound(x, y, width, height, style.Vars.TabRounding, bg, style.Vars.TabRoundingCorners);
    if (isActive)
        renderer->FillRect(x, y, width, 2, style.Colors.TabAccent);

    DrawTextInternal(title, x + 8, y + (height / 2) + 5, style.Colors.TabText, width - 16);
    return r;
}

// ---------------------------------------------------------------------------
// Checkbox
// ---------------------------------------------------------------------------

WidgetResult UIManager::Checkbox(const std::string& id_str,
                                 const std::string& label, bool& checked)
{
    UIID id = GetScopedID(id_str);
    const int boxSize = 14;
    const int paddingAfterBox = 8; // Increased from 5 to 8 for more breathing room
    auto& lc = Layout();
    int x = lc.cursorX, y = lc.cursorY;

    int labelW = MeasureText(label);
    int totalW = boxSize + paddingAfterBox + labelW;
    int totalH = boxSize;

    WidgetResult r;
    // Mouse interaction remains bound to the checkbox square
    r.hovered = IsMouseOver(x, y, boxSize, boxSize);
    if (r.hovered) {
        mouseOverAnyWidget = true;
        hotID = id;
        if (activeID == UI_ID_NONE && io.mouseLeftClicked) activeID = id;
    }
    if (hotID == id && activeID == id && !io.mouseLeftDown) {
        checked   = !checked;
        r.changed = r.activated = true;
    }

    Color bg = checked ? style.Colors.CheckboxChecked : style.Colors.CheckboxIdle;
    FillRectRound(x, y, boxSize, boxSize, style.Vars.CheckboxRounding, bg, style.Vars.CheckboxRoundingCorners);

    if (checked) {
        renderer->FillRect(x + 3, y + 7, 4, 2, style.Colors.CheckboxMark);
        renderer->FillRect(x + 6, y + 4, 2, 5, style.Colors.CheckboxMark);
    }

    // Fixed the final parameter: changed from (boxSize - 10) to labelW (or a proper font size)
    // so the text engine has enough horizontal/vertical space to render the string.
    DrawTextInternal(label, x + boxSize + paddingAfterBox, y + boxSize - 2, style.Colors.Text, labelW);

    AdvanceCursor(totalW, totalH);
    return r;
}
// ---------------------------------------------------------------------------
// ColorBadge
// ---------------------------------------------------------------------------

WidgetResult UIManager::ColorBadge(const std::string& label,
                                   Color bg, Color fg,
                                   int paddingX, int paddingH)
{
    int textW  = MeasureText(label);
    int badgeW = textW + paddingX * 2;
    int badgeH = 14 + paddingH * 2;

    auto& lc = Layout();
    int x = lc.cursorX, y = lc.cursorY;

    FillRectRound(x, y, badgeW, badgeH, style.Vars.BadgeRounding, bg, style.Vars.BadgeRoundingCorners);
    DrawTextInternal(label, x + paddingX, y + badgeH / 2 + 5, fg, badgeW - paddingX * 2);

    WidgetResult r;
    r.hovered = IsMouseOver(x, y, badgeW, badgeH);
    if (r.hovered) mouseOverAnyWidget = true;
    AdvanceCursor(badgeW, badgeH);
    return r;
}

// ---------------------------------------------------------------------------
// TextInputWidget  (shared core)
// ---------------------------------------------------------------------------

WidgetResult UIManager::TextInputWidget(UIID id, std::string& text,
                                        int x, int y, int w, int h)
{
    TextFieldState& s = GetTextFieldState(id);
    WidgetResult result;

    bool over = IsMouseOver(x, y, w, h);
    if (over) {
        mouseOverAnyWidget = true;
        hotID = id;
        if (io.mouseLeftClicked) {
            focusID = id;
            s.isDragging = true;
            s.selectStartIndex = -1;
        }
    } else if (io.mouseLeftClicked && focusID == id) {
        focusID = UI_ID_NONE;
        s.isDragging = false;
    }
    if (!io.mouseLeftDown) s.isDragging = false;
    result.hovered = over;
    result.held    = (focusID == id && s.isDragging);

    if (focusID == id) {
        bool hasSel = (s.selectStartIndex != -1 && s.selectStartIndex != s.cursorIndex);
        int  selMin = hasSel ? std::min(s.selectStartIndex, s.cursorIndex) : 0;
        int  selMax = hasSel ? std::max(s.selectStartIndex, s.cursorIndex) : 0;

        if (io.backspacePressed) {
            if (hasSel) {
                text.erase(selMin, selMax - selMin);
                s.cursorIndex = selMin;
                s.selectStartIndex = selMin;
            } else if (s.cursorIndex > 0) {
                text.erase(s.cursorIndex - 1, 1);
                --s.cursorIndex;
                s.selectStartIndex = s.cursorIndex;
            }
            result.changed = true;
        } else if (io.lastTypedChar >= 32 && io.lastTypedChar <= 126) {
            if (hasSel) {
                text.erase(selMin, selMax - selMin);
                s.cursorIndex = selMin;
            }
            text.insert(text.begin() + s.cursorIndex, io.lastTypedChar);
            ++s.cursorIndex;
            s.selectStartIndex = s.cursorIndex;
            result.changed = true;
        }

        s.cursorIndex = std::clamp(s.cursorIndex, 0, (int)text.size());
        if (io.enterPressed) result.activated = true;
    }

    // 1. Build per-character X positions
    std::vector<int> charX;
    charX.reserve(text.size() + 1);
    int penX = x + 8;
    char prev = '\0';
    charX.push_back(penX);

    s.cursorIndex      = std::clamp(s.cursorIndex,      0, (int)text.size());
    s.selectStartIndex = std::clamp(s.selectStartIndex, 0, (int)text.size());

    for (char c : text) {
        auto g = font.GetGlyph(IRenderBackend::GetRenderBackend().get(), c);
        if (prev != '\0') penX += font.GetKerning(c, prev).x >> 6;
        penX += g.advance;
        charX.push_back(penX);
        prev = c;
    }

    // 2. Handle Mouse Click & Drag Selection Indexes
    if (focusID == id && (io.mouseLeftClicked || s.isDragging)) {
        int target = 0, minD = 999999;
        for (int i = 0; i < (int)charX.size(); ++i) {
            int d = std::abs(io.mouseX - charX[i]);
            if (d < minD) { minD = d; target = i; }
        }
        if (io.mouseLeftClicked) {
            s.cursorIndex = target;
            s.selectStartIndex = target;
        } else {
            s.cursorIndex = target;
        }
    }

    // 3. Draw Background & Frame Bounds Safely
    Color Bg = (focusID == id) ? style.Colors.InputBg // Revert to stable fallback if theme color flashes
                               : style.Colors.InputBg;
    FillRectRound(x, y, w, h, style.Vars.InputRounding, Bg, style.Vars.InputRoundingCorners);

    // Apply border outline indicators clearly
    Color borderCol = (focusID == id) ? style.Colors.InputBorderFocus : style.Colors.InputBorderIdle;
    renderer->DrawRect(x, y, w, h, borderCol);

    // Restrict text selection and glyph overruns to inside the input panel text bounds
    PushClipRect(x + 4, y + 2, w - 8, h - 4);

    bool hasSel = (focusID == id && s.selectStartIndex != -1 && s.selectStartIndex != s.cursorIndex);
    int  selMin = hasSel ? std::min(s.selectStartIndex, s.cursorIndex) : 0;
    int  selMax = hasSel ? std::max(s.selectStartIndex, s.cursorIndex) : 0;

    // 4. Draw Highlight Background
    if (hasSel) {
        renderer->FillRect(charX[selMin], y + 4,
                           charX[selMax] - charX[selMin], h - 8,
                           style.Colors.InputSelection);
    }

    // 5. Draw Glyphs
    int drawX = x + 8;
    prev = '\0';
    int baseline = y + (h / 2) + 5;
    for (int i = 0; i < (int)text.size(); ++i) {
        char c = text[i];
        auto g = font.GetGlyph(IRenderBackend::GetRenderBackend().get(), c);
        if (g.width == 0 && c != ' ') { drawX += g.advance; continue; }
        if (prev != '\0') drawX += font.GetKerning(c, prev).x >> 6;

        Color tc = (hasSel && i >= selMin && i < selMax)
                   ? style.Colors.InputSelText : style.Colors.InputText;

        if (IsInsideClip(drawX + g.bearingX, baseline - g.bearingY, g.width, g.height)) {
            renderer->DrawGlyph(drawX + g.bearingX, baseline - g.bearingY, g, tc);
        }
        drawX += g.advance;
        prev = c;
    }

    // 6. --- FIXED BLINK LOGIC ---
    // Instead of relying on volatile per-frame state accumulations,
    // evaluate the timestamp directly through a clean 500ms math modulo mapping.
    uint64_t nowMs = static_cast<uint64_t>(NowMs());
    bool showCursor = (focusID == id) && ((nowMs / 500) % 2 == 0);

    if (showCursor && s.cursorIndex < (int)charX.size()) {
        int padT = 5, padB = 5;
        renderer->FillRect(charX[s.cursorIndex], y + padT, 2, h - padT - padB,
                           style.Colors.InputCursor);
    }

    PopClipRect();
    return result;
}
WidgetResult UIManager::TextField(const std::string& id_str, std::string& text,
                                  int width, int height)
{
    if (height < 0) height = style.Vars.DefaultItemHeight;
    UIID id = GetScopedID(id_str);
    auto& lc = Layout();
    int x = lc.cursorX, y = lc.cursorY;
    AdvanceCursor(width, height);
    return TextInputWidget(id, text, x, y, width, height);
}

WidgetResult UIManager::AddressBar(const std::string& id_str, std::string& text,
                                   int width, int height)
{
    if (height < 0) height = style.Vars.DefaultItemHeight;
    UIID id = GetScopedID(id_str);
    auto& lc = Layout();
    int x = lc.cursorX, y = lc.cursorY;
    AdvanceCursor(width, height);
    return TextInputWidget(id, text, x, y, width, height);
}

void UIManager::Text(const std::string& id_str,
                     const std::string& text,
                     int width,
                     int height)
{
    auto& lc = Layout();

    int x = lc.cursorX;
    int y = lc.cursorY;

    int textW = MeasureText(text);

    if (width <= 0)
        width = textW;

    if (height <= 0)
        height = 16;

    int baseline = y + height / 2 + 5;

    DrawTextInternal(
        text,
        x,
        baseline,
        style.Colors.Text,
        width
    );

    AdvanceCursor(width, height);
}

// ---------------------------------------------------------------------------
// ScrollableList
// ---------------------------------------------------------------------------

bool UIManager::BeginListBox(const std::string& id_str, int width, int height,
                             bool flip, int rowHeight)
{
    UIID id  = GetScopedID(id_str);
    auto& ls = listBoxStates[id];
    auto& lc = Layout();

    int x = lc.cursorX, y = lc.cursorY;
    AdvanceCursor(width, height);

    // Scroll
    if (IsMouseOver(x, y, width, height) && io.mouseWheelDelta != 0) {

        int scrollMod = flip ? 1 : -1;

        ls.scrollOffsetY -=
            io.mouseWheelDelta * rowHeight * 3 * scrollMod;

        // clamp immediately using previous-frame content size
        int maxScroll = std::max(0, ls.contentHeight - height);

        ls.scrollOffsetY =
            std::clamp(ls.scrollOffsetY, 0, maxScroll);
    }

    renderer->FillRect(x, y, width, height, style.Colors.ListBg);
    renderer->DrawRect(x, y, width, height, style.Colors.ListBorder);

    // NOTE: Ensure your renderer implements actual scissor testing here!
    PushClipRect(x, y, width, height);

    // Set up context for Selectable calls
    activeListBox = {id, x, y, width, height, rowHeight, 0, flip, &ls};
    inListBox     = true;

    // Push inner layout — Selectable will use this cursor
    LayoutContext inner;
    inner.originX  = inner.cursorX = x;
    inner.originY  = inner.cursorY = y - ls.scrollOffsetY;
    inner.contentW = width;
    layoutStack.push_back(inner);

    return true;
}

void UIManager::EndListBox() {
    if (!inListBox) return;

    // Clamp scroll now that we know total content height
    auto& lb  = activeListBox;
    auto& ls  = *lb.state;
    int totalH    = lb.visibleRow * lb.rowHeight;
    ls.contentHeight = totalH;
    int maxScroll = std::max(0, totalH - lb.h);
    ls.scrollOffsetY =
    std::clamp(ls.scrollOffsetY, 0, maxScroll);

    // Scrollbar
    if (totalH > lb.h) {
        const int sbW = 5;
        float ratio = (float)lb.h / (float)totalH;
        int   sbH   = std::max(20, (int)(lb.h * ratio));
        float t     = maxScroll > 0 ? (float)ls.scrollOffsetY / (float)maxScroll : 0.f;
        int   sbY   = lb.y + (int)((lb.h - sbH) * t);

        renderer->FillRect(lb.x + lb.w - sbW, lb.y, sbW, lb.h, style.Colors.ListScrollTrack);
        renderer->FillRect(lb.x + lb.w - sbW, sbY,  sbW, sbH,  style.Colors.ListScrollThumb);
    }

    layoutStack.pop_back();
    PopClipRect();
    inListBox = false;


}

bool UIManager::Selectable(const std::string& id_str, const std::string& text,bool selected,
                            int width, int height, Color textColor)
{
    UIID  id  = GetScopedID(id_str);
    auto& lc  = Layout();

    int x = lc.cursorX;
    int y = lc.cursorY;
    int w = (width  > 0) ? width  : (lc.contentW > 0 ? lc.contentW : windowWidth);
    int h = (height > 0) ? height : style.Vars.DefaultItemHeight;

    // Delegate row tracking to the context
    int row = inListBox ? activeListBox.visibleRow++ : -1;
    auto* ls = inListBox ? activeListBox.state : nullptr;

    // FIX 3: Software Culling to prevent clipping into other UI elements
    if (inListBox) {
        auto& lb = activeListBox;
        if (y + h <= lb.y || y >= lb.y + lb.h) {
            AdvanceCursor(w, h); // Must still advance cursor for layout!
            return false;        // Skip rendering and interaction completely
        }
    }

    Color rowBg = selected
        ? style.Colors.ListRowSelected
        : (row >= 0 && row % 2 == 1 ? style.Colors.ListRowOdd : style.Colors.ListRowEven);

    // Outside a list, unselected selectables are transparent by default
    if (!inListBox && !selected) rowBg = {0, 0, 0, 0};

    renderer->FillRect(x, y, w, h, rowBg);

    bool hovered = IsMouseOver(x, y, w, h);
    if (hovered) {
        mouseOverAnyWidget = true;
        if (!selected)
            renderer->FillRect(x, y, w, h, style.Colors.ListRowHover);
    }

    bool clicked = hovered && io.mouseLeftClicked;
    if (clicked && ls) ls->selectedRow = row;

    AdvanceCursor(w, h);

    Color tc = (textColor.a == 0) ? style.Colors.Text : textColor;
    DrawTextInternal(text, x + 6, y + h / 2 + 5, tc, w - 12);

    return clicked;
}

int UIManager::GetListSelection(const std::string& id_str) {
    UIID id = GetScopedID(id_str);

    auto it = listStates.find(id);
    if (it != listStates.end()) return it->second.selectedRow;

    auto it2 = listBoxStates.find(id);
    if (it2 != listBoxStates.end()) return it2->second.selectedRow;

    return -1;
}

void UIManager::ClearListSelection(const std::string& id_str) {
    UIID id = GetScopedID(id_str);

    auto it = listStates.find(id);
    if (it != listStates.end()) { it->second.selectedRow = -1; return; }

    auto it2 = listBoxStates.find(id);
    if (it2 != listBoxStates.end()) it2->second.selectedRow = -1;
}

void UIManager::ScrollListToBottom(const std::string& id_str,
                                   int rows,
                                   int viewportHeight, int rowHeight)
{
    UIID id       = GetScopedID(id_str);
    int  totalH   = rows * rowHeight;
    int  maxScroll = std::max(0, totalH - viewportHeight);

    auto it = listStates.find(id);
    if (it != listStates.end()) { it->second.scrollOffsetY = maxScroll; return; }

    auto it2 = listBoxStates.find(id);
    if (it2 != listBoxStates.end()) it2->second.scrollOffsetY = maxScroll;
}

bool UIManager::TreeNode(const std::string& id_str,
                         const std::string& label, bool &clicked, bool Selected,
                         int width,
                         int height)
{
    if (height < 0)
        height = style.Vars.DefaultItemHeight;

    UIID id = GetScopedID(id_str);
    auto& state = treeNodeStates[id];

    state.id = id;

    auto& lc = Layout();

    if (width <= 0) {
        width = (lc.contentW > 0)
            ? (lc.contentW - (lc.cursorX - lc.originX))
            : 200;
    }

    int x = lc.cursorX;
    int y = lc.cursorY;

    bool hovered = IsMouseOver(x + 14, y, width - 14, height);

    if (hovered) {
        mouseOverAnyWidget = true;
        hotID = id;

        if (IsMouseClicked())
        {

            activeID = id;
        }
    }

    clicked =
        (hotID == id &&
         activeID == id &&
         IsMouseClicked());

    if (IsMouseOver(x, y, 14, height) && IsMouseClicked())
        state.expanded = !state.expanded;

    // ---------------------------------------------------------
    // background
    // ---------------------------------------------------------
    if (Selected) {
        renderer->FillRect(
            x,
            y,
            width,
            height,
            style.Colors.ListRowSelected
        );
    }
    else if (hovered) {
        renderer->FillRect(
            x,
            y,
            width,
            height,
            style.Colors.ListRowHover
        );
    }

    // ---------------------------------------------------------
    // disclosure triangle
    // ---------------------------------------------------------
    int arrowX = x + 4;
    int arrowY = y + height / 2;

    Color arrowColor = style.Colors.Text;

    if (state.expanded) {
        // ▼
        renderer->DrawLine(
            arrowX,
            arrowY - 2,
            arrowX + 8,
            arrowY - 2,
            arrowColor
        );

        renderer->DrawLine(
            arrowX + 1,
            arrowY - 1,
            arrowX + 7,
            arrowY - 1,
            arrowColor
        );

        renderer->DrawLine(
            arrowX + 2,
            arrowY,
            arrowX + 6,
            arrowY,
            arrowColor
        );
    }
    else {
        // ▶
        renderer->DrawLine(
            arrowX,
            arrowY - 4,
            arrowX,
            arrowY + 4,
            arrowColor
        );

        renderer->DrawLine(
            arrowX + 1,
            arrowY - 3,
            arrowX + 1,
            arrowY + 3,
            arrowColor
        );

        renderer->DrawLine(
            arrowX + 2,
            arrowY - 2,
            arrowX + 2,
            arrowY + 2,
            arrowColor
        );
    }

    // ---------------------------------------------------------
    // label
    // ---------------------------------------------------------
    DrawTextInternal(
        label,
        x + 18,
        y + (height / 2) + 5,
        style.Colors.Text,
        width - 18
    );

    AdvanceCursor(width, height);

    // ---------------------------------------------------------
    // indent children automatically
    // ---------------------------------------------------------
    if (state.expanded)
        Indent(style.Vars.TreeNodeSpacingX);

    return state.expanded;
}

// -----------------------------------------------------------------------------
// UIManager::TreePop
// -----------------------------------------------------------------------------
void UIManager::TreePop(const std::string& id_str)
{
    UIID id = GetScopedID(id_str);

    auto it = treeNodeStates.find(id);

    if (it == treeNodeStates.end())
        return;

    if (it->second.expanded)
        Unindent(style.Vars.TreeNodeSpacingX);
}

bool UIManager::IsTreeOpen(const std::string &id_str) const {
    UIID id = GetScopedID(id_str);
    auto it = treeNodeStates.find(id);
    if (it == treeNodeStates.end())
        return false;
    return it->second.expanded;
}

WidgetResult UIManager::ProgressBar(
    float t,
    int width,
    int height)
{
    auto& lc = Layout();
    int x = lc.cursorX;
    int y = lc.cursorY;

    t = std::clamp(t, 0.0f, 1.0f);

    FillRectRound(
        x, y,
        width, height,
        style.Vars.ButtonRounding,
        style.Colors.InputBg,
        Corner_All);

    FillRectRound(
        x, y,
        (int)(width * t), height,
        style.Vars.ButtonRounding,
        style.Colors.TabAccent,
        Corner_All);

    AdvanceCursor(width, height);

    WidgetResult r;
    r.hovered = IsMouseOver(x, y, width, height);
    return r;
}
