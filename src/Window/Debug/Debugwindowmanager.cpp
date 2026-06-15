#include "DebugWindowManager.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

// Your project's Platform factory and Node definition
#include <ranges>
#include <source_location>

#include "../../Curl/CurlGrabber.h"
#include "../../Debug/Logger.h"
#include "Platform/Platform.h"
#include "Render/Renderer.h"
#include "../../Node/Node.h"

// ---------------------------------------------------------------------------
// Helpers (Light Mode Palette)
// ---------------------------------------------------------------------------
static std::string LevelBadgeStr(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Fatal: return "FTL";
        case LogLevel::Error: return "ERR";
        case LogLevel::Warning: return "WRN";
        case LogLevel::Info: return "INF";
        case LogLevel::Debug: return "DBG";
        case LogLevel::Verbose: return "VRB";
        case LogLevel::None: default: return "LOG";
    }
}

static Color LevelBadgeBg(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Fatal: return {215, 215, 255};
        case LogLevel::Error: return {225, 225, 255};
        case LogLevel::Warning: return {205, 243, 255};
        case LogLevel::Info: return {252, 244, 207};
        case LogLevel::Debug: return {255, 230, 243};
        case LogLevel::None:
        case LogLevel::Verbose:
        default: return {245, 242, 240};
    }
}

static Color LevelBadgeText(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Fatal: return {230, 220, 255};
        case LogLevel::Error: return {240, 230, 255};
        case LogLevel::Warning: return {255, 245, 255};
        case LogLevel::Info: return {255, 250, 245};
        case LogLevel::Debug: return {255, 245, 255};
        default: return {255, 220, 230};
    }
}

static Color LevelRowTint(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Fatal: return {35, 20, 55};
        case LogLevel::Error: return {45, 25, 65};
        case LogLevel::Warning: return {82, 42, 58};
        default: return {48, 24, 32};
    }
}

static Color StatusBadgeBg(int code) {
    if (code == 0) return {95, 55, 70};
    if (code >= 500) return {95, 55, 150};
    if (code >= 400) return {255, 110, 180};
    if (code >= 300) return {180, 90, 110};
    return {170, 210, 110};
}

static Color StatusBadgeText(int code) {
    if (code == 0) return {255, 220, 230};
    if (code >= 500) return {245, 235, 255};
    if (code >= 400) return {255, 245, 255};
    if (code >= 300) return {255, 230, 240};
    return {30, 40, 20};
}

static bool LevelHasTint(LogLevel lvl) {
    return (lvl == LogLevel::Fatal || lvl == LogLevel::Error || lvl == LogLevel::Warning);
}

static std::string StatusStr(int code) {
    if (code == 0) return "—";
    return std::to_string(code);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

DebugWindowManager::DebugWindowManager(int width, int height)
    : renderBackend(IRenderBackend::GetRenderBackend()),
      windowWidth(width),
      windowHeight(height) {
}

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------

bool DebugWindowManager::Open() {
    if (isOpen) return true;

    platform = CreatePlatform();
    if (!platform->OpenWindow(windowWidth, windowHeight, "DevTools", false)) {
        platform.reset();
        return false;
    }
    platform->SetMinimumSize(600, 380);

    ui = std::make_unique<UIManager>(windowWidth, windowHeight);

    renderWindow = renderBackend->RegisterWindow(platform.get());
    renderBackend->AttachRenderTarget(renderWindow, ui->GetRenderer()->GetTargetID());

    minimize = ui->MakeImage("./Assets/Icons/minus.svg", 20, 20);
    maximize = ui->MakeImage("./Assets/Icons/maximize.svg", 20, 20);
    Return = ui->MakeImage("./Assets/Icons/minimize.svg", 20, 20);

    // Mark all data dirty so first frame fully rebuilds everything
    consoleRowsDirty = true;
    networkRowsDirty = true;
    inspectorDirty = true;

    isOpen = true;
    return true;
}

void DebugWindowManager::Close() {
    if (!isOpen) return;
    if (renderBackend && renderWindow != 0) {
        renderBackend->UnregisterWindow(renderWindow);
        renderWindow = 0;
    }
    platform.reset();
    ui.reset();
    isOpen = false;
}

// ---------------------------------------------------------------------------
// Data feeds
// ---------------------------------------------------------------------------

void DebugWindowManager::FeedDOM(const Node *root) {
    if (domRoot == root) return;
    domRoot = root;
    selectedNode = nullptr;
    styleRows.clear();
    inspectorDirty = true;
}

void DebugWindowManager::FeedJS(JavaScriptEngine *engine) {
    jsEngine = engine;
}

void DebugWindowManager::PushLog(LogLevel level,
                                 const std::string &message,
                                 const std::string &source, int indent) {
    logEntries.push_back({level, message, source, indent});
    consoleRowsDirty = true;
}

void DebugWindowManager::ClearLogs() {
    logEntries.clear();
    consoleRows.clear();
    consoleRowsDirty = false;
    if (ui) ui->ClearListSelection("console_list");
}

Platform *DebugWindowManager::GetPlatform() const {
    return platform.get();
}

void DebugWindowManager::SetNetworkEntries(const std::vector<DebugNetEntry> &entries) {
    netEntries = entries;
    networkRowsDirty = true;
}

// ---------------------------------------------------------------------------
// Row builders
// ---------------------------------------------------------------------------

void DebugWindowManager::RebuildConsoleRows() {
    consoleRows.clear();
    for (const auto &e: logEntries) {
        if (e.level == LogLevel::Verbose && !showLogs) continue;
        if (e.level == LogLevel::Debug && !showLogs) continue;
        if (e.level == LogLevel::Info && !showLogs) continue;
        if (e.level == LogLevel::Warning && !showWarns) continue;
        if (e.level == LogLevel::Error && !showErrors) continue;
        if (e.level == LogLevel::Fatal && !showErrors) continue;

        if (!consoleFilter.empty()) {
            std::string lower = e.message;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            std::string filterLower = consoleFilter;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
            if (lower.find(filterLower) == std::string::npos) continue;
        }

        DebugListRow row;
        row.text = e.message;
        row.annotation = e.source;
        row.badgeLabel = LevelBadgeStr(e.level);
        row.badgeColor = LevelBadgeBg(e.level);
        row.badgeText = LevelBadgeText(e.level);
        row.rowTint = LevelRowTint(e.level);
        row.hasTint = LevelHasTint(e.level);
        row.textColor = {235, 225, 255}; // Dark text for list rows
        consoleRows.push_back(std::move(row));
    }
    consoleRowsDirty = false;
}

void DebugWindowManager::RebuildNetworkRows() {
    networkRows.clear();
    for (const auto &e: netEntries) {
        DebugListRow row;

        row.badgeLabel = e.method.empty() ? "GET" : e.method;
        row.badgeColor = {230, 240, 255}; // Faint light blue for methods
        row.badgeText = {30, 80, 180}; // High-contrast blue text

        row.text = e.url;
        row.annotation = StatusStr(e.statusCode);
        row.badgeColor = StatusBadgeBg(e.statusCode);
        row.badgeLabel = StatusStr(e.statusCode);
        row.badgeText = StatusBadgeText(e.statusCode);
        row.textColor = {235, 225, 255};

        std::string ann;
        if (!e.contentType.empty()) ann += e.contentType + "  ";
        if (e.sizeBytes > 0) {
            if (e.sizeBytes >= 1024)
                ann += std::to_string(e.sizeBytes / 1024) + " KB";
            else
                ann += std::to_string(e.sizeBytes) + " B";
        }
        if (e.timeMs > 0) ann += "  " + std::to_string(e.timeMs) + "ms";
        row.annotation = ann;

        row.hasTint = true;
        row.rowTint = (e.statusCode >= 500)
                          ? Color{235, 235, 255}
                          : (e.statusCode >= 400)
                                ? Color{230, 250, 255}
                                : Color{255, 255, 255};
        networkRows.push_back(std::move(row));
    }
    networkRowsDirty = false;
}


void DebugWindowManager::RebuildInspectorRows() {
    inspectorRows.clear();
    inspectorNodes.clear();
    inspectorDirty = false;
}

void DebugWindowManager::BuildStyleRows() {
    styleRows.clear();
    if (!selectedNode) return;

    // ----------------------------------------------------------------
    // String converters
    // ----------------------------------------------------------------

    auto DisplayStr = [](DisplayType d) -> std::string {
        switch (d) {
            case DisplayType::Block:       return "block";
            case DisplayType::Inline:      return "inline";
            case DisplayType::InlineBlock: return "inline-block";
            case DisplayType::Flex:        return "flex";
            case DisplayType::Grid:        return "grid";
            case DisplayType::None:        return "none";
            default:                       return "unknown";
        }
    };

    auto PositionStr = [](PositionType p) -> std::string {
        switch (p) {
            case PositionType::Static:   return "static";
            case PositionType::Relative: return "relative";
            case PositionType::Absolute: return "absolute";
            default:                     return "unknown";
        }
    };

    auto OverflowStr = [](OverflowType o) -> std::string {
        switch (o) {
            case OverflowType::Visible: return "visible";
            case OverflowType::Hidden:  return "hidden";
            case OverflowType::Scroll:  return "scroll";
            default:                    return "unknown";
        }
    };

    auto ColorStr = [](const Color& c) -> std::string {
        return "rgb(" + std::to_string(c.r) + ", "
                      + std::to_string(c.g) + ", "
                      + std::to_string(c.b) + ")";
    };

    auto BoxSizingStr = [](BoxSizing b) -> std::string {
        return (b == BoxSizing::BorderBox) ? "border-box" : "content-box";
    };

    auto WhiteSpaceStr = [](WhiteSpace w) -> std::string {
        switch (w) {
            case WhiteSpace::Normal:  return "normal";
            case WhiteSpace::NoWrap:  return "nowrap";
            case WhiteSpace::Pre:     return "pre";
            case WhiteSpace::PreWrap: return "pre-wrap";
            case WhiteSpace::PreLine: return "pre-line";
            default:                  return "unknown";
        }
    };

    auto TextOverflowStr = [](TextOverflow t) -> std::string {
        return (t == TextOverflow::Ellipsis) ? "ellipsis" : "clip";
    };

    auto TextAlignStr = [](TextAlign t) -> std::string {
        switch (t) {
            case TextAlign::Left:   return "left";
            case TextAlign::Center: return "center";
            case TextAlign::Right:  return "right";
            default:                return "left";
        }
    };

    auto ObjectFitStr = [](ObjectFit o) -> std::string {
        switch (o) {
            case ObjectFit::Fill:      return "fill";
            case ObjectFit::Contain:   return "contain";
            case ObjectFit::Cover:     return "cover";
            case ObjectFit::ScaleDown: return "scale-down";
            case ObjectFit::None:      return "none";
            default:                   return "fill";
        }
    };

    auto VerticalAlignStr = [](VerticalAlignKeyword v) -> std::string {
        switch (v) {
            case VerticalAlignKeyword::Top:         return "top";
            case VerticalAlignKeyword::Middle:      return "middle";
            case VerticalAlignKeyword::Bottom:      return "bottom";
            case VerticalAlignKeyword::Sub:         return "sub";
            case VerticalAlignKeyword::Super:       return "super";
            case VerticalAlignKeyword::TextTop:     return "text-top";
            case VerticalAlignKeyword::TextBottom:  return "text-bottom";
            case VerticalAlignKeyword::Baseline:    return "baseline";
            default:                                return "other";
        }
    };

    auto LengthStr = [](const CSSLength& len) -> std::string {
        if (len.unit == LengthUnit::Auto)    return "auto";
        if (len.unit == LengthUnit::Inherit) return "inherit";

        std::string val = std::to_string(len.value);
        val.erase(val.find_last_not_of('0') + 1, std::string::npos);
        if (val.back() == '.') val.pop_back();

        switch (len.unit) {
            case LengthUnit::Px:      return val + "px";
            case LengthUnit::Percent: return val + "%";
            case LengthUnit::Em:      return val + "em";
            case LengthUnit::Vh:      return val + "vh";
            case LengthUnit::Vw:      return val + "vw";
            default:                  return val;
        }
    };

    auto FloatStr = [](float v) -> std::string {
        std::string s = std::to_string(v);
        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
        if (s.back() == '.') s.pop_back();
        return s;
    };

    // Flex enum converters
    auto FlexDirectionStr = [](FlexDirection d) -> std::string {
        switch (d) {
            case FlexDirection::Row:           return "row";
            case FlexDirection::RowReverse:    return "row-reverse";
            case FlexDirection::Column:        return "column";
            case FlexDirection::ColumnReverse: return "column-reverse";
            default:                           return "row";
        }
    };

    auto FlexWrapStr = [](FlexWrap w) -> std::string {
        switch (w) {
            case FlexWrap::NoWrap:      return "nowrap";
            case FlexWrap::Wrap:        return "wrap";
            case FlexWrap::WrapReverse: return "wrap-reverse";
            default:                    return "nowrap";
        }
    };

    auto JustifyContentStr = [](JustifyContent j) -> std::string {
        switch (j) {
            case JustifyContent::Start:        return "start";
            case JustifyContent::End:          return "end";
            case JustifyContent::Center:       return "center";
            case JustifyContent::FlexStart:    return "flex-start";
            case JustifyContent::FlexEnd:      return "flex-end";
            case JustifyContent::Left:         return "left";
            case JustifyContent::Right:        return "right";
            case JustifyContent::SpaceBetween: return "space-between";
            case JustifyContent::SpaceAround:  return "space-around";
            case JustifyContent::SpaceEvenly:  return "space-evenly";
            case JustifyContent::Stretch:      return "stretch";
            default:                           return "flex-start";
        }
    };

    auto AlignItemsStr = [](AlignItems a) -> std::string {
        switch (a) {
            case AlignItems::Normal:        return "normal";
            case AlignItems::Start:         return "start";
            case AlignItems::End:           return "end";
            case AlignItems::Center:        return "center";
            case AlignItems::FlexStart:     return "flex-start";
            case AlignItems::FlexEnd:       return "flex-end";
            case AlignItems::SelfStart:     return "self-start";
            case AlignItems::SelfEnd:       return "self-end";
            case AlignItems::Baseline:      return "baseline";
            case AlignItems::FirstBaseline: return "first baseline";
            case AlignItems::LastBaseline:  return "last baseline";
            case AlignItems::Stretch:       return "stretch";
            default:                        return "stretch";
        }
    };

    auto AlignSelfStr = [](AlignSelf a) -> std::string {
        switch (a) {
            case AlignSelf::Auto:          return "auto";
            case AlignSelf::Normal:        return "normal";
            case AlignSelf::Start:         return "start";
            case AlignSelf::End:           return "end";
            case AlignSelf::Center:        return "center";
            case AlignSelf::FlexStart:     return "flex-start";
            case AlignSelf::FlexEnd:       return "flex-end";
            case AlignSelf::SelfStart:     return "self-start";
            case AlignSelf::SelfEnd:       return "self-end";
            case AlignSelf::Baseline:      return "baseline";
            case AlignSelf::FirstBaseline: return "first baseline";
            case AlignSelf::LastBaseline:  return "last baseline";
            case AlignSelf::Stretch:       return "stretch";
            default:                       return "auto";
        }
    };

    auto AlignContentStr = [](AlignContent a) -> std::string {
        switch (a) {
            case AlignContent::Normal:        return "normal";
            case AlignContent::Start:         return "start";
            case AlignContent::End:           return "end";
            case AlignContent::Center:        return "center";
            case AlignContent::FlexStart:     return "flex-start";
            case AlignContent::FlexEnd:       return "flex-end";
            case AlignContent::SpaceBetween:  return "space-between";
            case AlignContent::SpaceAround:   return "space-around";
            case AlignContent::SpaceEvenly:   return "space-evenly";
            case AlignContent::Stretch:       return "stretch";
            case AlignContent::Baseline:      return "baseline";
            case AlignContent::FirstBaseline: return "first baseline";
            case AlignContent::LastBaseline:  return "last baseline";
            default:                          return "normal";
        }
    };

    auto OverflowAlignmentStr = [](OverflowAlignment o) -> std::string {
        switch (o) {
            case OverflowAlignment::Safe:   return "safe";
            case OverflowAlignment::Unsafe: return "unsafe";
            default:                        return "default";
        }
    };

    // ----------------------------------------------------------------
    // Row helpers
    // ----------------------------------------------------------------

    auto addHeader = [&](const std::string& label) {
        DebugListRow r;
        r.text       = label;
        r.badgeLabel = "CSS";
        r.badgeColor = {170, 110, 255};
        r.badgeText  = {255, 245, 255};
        r.textColor  = {240, 230, 255};
        styleRows.push_back(r);
    };

    auto addRow = [&](const std::string& prop, const std::string& val) {
        DebugListRow r;
        r.text      = prop + ": " + val;
        r.textColor = {220, 210, 255};
        styleRows.push_back(r);
    };

    // ----------------------------------------------------------------
    // Computed styles
    // ----------------------------------------------------------------

    const Style& cs = selectedNode->computedStyle;

    addHeader("computed style");

    if (cs.set.display)       addRow("display",      DisplayStr(cs.display));
    if (cs.set.color)         addRow("color",         ColorStr(cs.color));
    if (cs.set.background)    addRow("background",    cs.hasBackground ? ColorStr(cs.backgroundColor) : "none");
    if (cs.set.font_size)     addRow("font-size",     LengthStr(cs.font_size));
    if (cs.set.font_bold)     addRow("font-weight",   cs.font_bold   ? "bold"   : "normal");
    if (cs.set.font_italic)   addRow("font-style",    cs.font_italic ? "italic" : "normal");

    if (cs.set.width)         addRow("width",         LengthStr(cs.width));
    if (cs.set.height)        addRow("height",        LengthStr(cs.height));
    if (cs.set.min_width)     addRow("min-width",     LengthStr(cs.min_width));
    if (cs.set.max_width)     addRow("max-width",     LengthStr(cs.max_width));
    if (cs.set.min_height)    addRow("min-height",    LengthStr(cs.min_height));
    if (cs.set.max_height)    addRow("max-height",    LengthStr(cs.max_height));

    if (cs.set.margin_top)    addRow("margin-top",    LengthStr(cs.margin_top));
    if (cs.set.margin_bottom) addRow("margin-bottom", LengthStr(cs.margin_bottom));
    if (cs.set.margin_left)   addRow("margin-left",   LengthStr(cs.margin_left));
    if (cs.set.margin_right)  addRow("margin-right",  LengthStr(cs.margin_right));

    if (cs.set.padding_top)    addRow("padding-top",    LengthStr(cs.padding_top));
    if (cs.set.padding_bottom) addRow("padding-bottom", LengthStr(cs.padding_bottom));
    if (cs.set.padding_left)   addRow("padding-left",   LengthStr(cs.padding_left));
    if (cs.set.padding_right)  addRow("padding-right",  LengthStr(cs.padding_right));

    if (cs.set.overflow)      addRow("overflow",      OverflowStr(cs.overflow));
    if (cs.set.boxSizing)     addRow("box-sizing",    BoxSizingStr(cs.boxSizing));
    if (cs.set.whiteSpace)    addRow("white-space",   WhiteSpaceStr(cs.whiteSpace));
    if (cs.set.textOverflow)  addRow("text-overflow", TextOverflowStr(cs.textOverflow));
    if (cs.set.textAlign)     addRow("text-align",    TextAlignStr(cs.textAlign));
    if (cs.set.objectFit)     addRow("object-fit",    ObjectFitStr(cs.objectFit));

    if (cs.set.verticalAlign) {
        addRow("vertical-align",
            cs.verticalAlign == VerticalAlignKeyword::Other
                ? LengthStr(cs.verticalAlignValue)
                : VerticalAlignStr(cs.verticalAlign));
    }

    if (cs.set.border_radius_top_left)     addRow("border-top-left-radius",     LengthStr(cs.border_radius_top_left));
    if (cs.set.border_radius_top_right)    addRow("border-top-right-radius",    LengthStr(cs.border_radius_top_right));
    if (cs.set.border_radius_bottom_right) addRow("border-bottom-right-radius", LengthStr(cs.border_radius_bottom_right));
    if (cs.set.border_radius_bottom_left)  addRow("border-bottom-left-radius",  LengthStr(cs.border_radius_bottom_left));

    if (cs.position != PositionType::Static)
        addRow("position", PositionStr(cs.position));

    // ----------------------------------------------------------------
    // Flex (shown only when this node is a flex container or item)
    // ----------------------------------------------------------------

    if (cs.display == DisplayType::Flex) {
        addHeader("flex container");

        const FlexData& f = cs.flex;
        addRow("flex-direction",    FlexDirectionStr(f.direction));
        addRow("flex-wrap",         FlexWrapStr(f.wrap));
        addRow("justify-content",   JustifyContentStr(f.justifyContent));
        addRow("align-items",       AlignItemsStr(f.alignItems));
        addRow("align-content",     AlignContentStr(f.alignContent));
        addRow("row-gap",           LengthStr(f.rowGap));
        addRow("column-gap",        LengthStr(f.columnGap));

        if (f.justifyOverflow != OverflowAlignment::Default)
            addRow("justify-overflow", OverflowAlignmentStr(f.justifyOverflow));
        if (f.alignOverflow != OverflowAlignment::Default)
            addRow("align-overflow",   OverflowAlignmentStr(f.alignOverflow));
    }

    // Show flex item properties if parent is a flex container
    const bool parentIsFlex = selectedNode->parent
        && selectedNode->parent->computedStyle.display == DisplayType::Flex;

    if (parentIsFlex) {
        addHeader("flex item");

        const FlexData& f = cs.flex;
        addRow("order",       std::to_string(f.order));
        addRow("flex-grow",   FloatStr(f.grow));
        addRow("flex-shrink", FloatStr(f.shrink));
        addRow("flex-basis",  LengthStr(f.basis));
        addRow("align-self",  AlignSelfStr(f.alignSelf));
    }

    // ----------------------------------------------------------------
    // Resolved / computed box model
    // ----------------------------------------------------------------

    addHeader("layout");

    const RenderData& rd = selectedNode->renderData;
    const float contentW = rd.box.width  - rd.resolved_padding_left - rd.resolved_padding_right;
    const float contentH = rd.box.height - rd.resolved_padding_top  - rd.resolved_padding_bottom;

    addRow("width",   FloatStr(contentW));
    addRow("height",  FloatStr(contentH));
    addRow("padding",
        FloatStr(rd.resolved_padding_top)    + " " +
        FloatStr(rd.resolved_padding_right)  + " " +
        FloatStr(rd.resolved_padding_bottom) + " " +
        FloatStr(rd.resolved_padding_left));
    addRow("margin",
        FloatStr(rd.resolved_margin_top)    + " " +
        FloatStr(rd.resolved_margin_right)  + " " +
        FloatStr(rd.resolved_margin_bottom) + " " +
        FloatStr(rd.resolved_margin_left));
}

void DebugWindowManager::RenderConsoleTab(int px, int py, int pw, int ph) {
    ui->SetCursor(px + 4, py + 4);

    bool filterChanged = false;
    if (ui->TextField("console_filter", consoleFilter, pw - 260, 24)) {
        filterChanged = true;
    }
    ui->SameLine(8);

    bool prevLogs = showLogs;
    bool prevWarns = showWarns;
    bool prevErrors = showErrors;

    ui->Checkbox("cb_log", "Log", showLogs);
    ui->SameLine(8);
    ui->Checkbox("cb_warn", "Warn", showWarns);
    ui->SameLine(8);
    ui->Checkbox("cb_err", "Err", showErrors);

    if (showLogs != prevLogs || showWarns != prevWarns || showErrors != prevErrors || filterChanged) {
        consoleRowsDirty = true;
    }

    ui->NewLine(4);

    ui->SetCursor(px + pw - 52, py + 4);
    if (ui->Button("Clear", 48, 24).activated) {
        ClearLogs();
        platform->needsRedraw = true;
    }

    int jsInputH = 32;
    int listY = py + TOOL_BAR_H;
    int listH = ph - TOOL_BAR_H - jsInputH;

    if (consoleRowsDirty) {
        platform->needsRedraw = true;
        RebuildConsoleRows();
        if (consoleAutoScroll) {
            ui->ScrollListToBottom("console_list", consoleRows.size(), listH, 22);
        }
    }

    ui->SetCursor(px, listY);
    int SelectedItem = ui->GetListSelection("console_list");
    if (ui->BeginListBox("console_list", pw, listH, true, 22)) {
        for (int i = 0; i < (int) consoleRows.size(); ++i) {
            ui->PushID("Console_Tab_" + std::to_string(i));
            ui->PushStyleColor(UIColorVar::Text, consoleRows[i].textColor);


            auto rowStart = ui->GetCursor();

            // background/selectable FIRST
            bool clicked = ui->Selectable(
                "Selector", "",
                i == SelectedItem,
                pw,
                20,
                Color(0, 0, 0, 0)
            );

            // restore cursor for overlay content
            ui->SetCursor(rowStart.x + 6, rowStart.y + 2);

            ui->Text("Badge", consoleRows[i].badgeLabel);
            ui->SameLine(10);
            ui->Text("Text", consoleRows[i].text);

            ui->SetCursor(px + pw - ui->MeasureText(consoleRows[i].annotation) - 10, rowStart.y + 2);
            ui->PushStyleColor(UIColorVar::Text, Color(80, 80, 80, 255));
            ui->Text("Annotation", consoleRows[i].annotation);
            ui->PopStyleColor();
            ui->NewLine();

            ui->PopID();
        }

        ui->EndListBox();
    }

    int jsY = py + ph - jsInputH + 4;
    ui->SetCursor(px + 4, jsY);

    int buttonW = 50;
    int inputW = pw - buttonW - 16;

    WidgetResult jsSubmitted = ui->TextField("js_input_bar", jsBuffer, inputW, 24);
    ui->SameLine(8);

    if (jsSubmitted || ui->Button("Run", buttonW, 24).activated) {
        if (!jsBuffer.empty()) {
            if (jsEngine) {
                Logger::Log("> " + jsBuffer, "Console Input", 0);
                auto result = jsEngine->Run(jsBuffer, "Console.js");
                Logger::Log(result, "Console Output", 0);
            } else {
                Logger::Log_Warning("JS Engine not attached.", "Debugger", 0);
            }

            jsBuffer.clear();
            consoleRowsDirty = true;
            platform->needsRedraw = true;
        }
    }
}

void DebugWindowManager::RenderNetworkTab(int px, int py, int pw, int ph) {
    ui->SetCursor(px + pw - 52, py + 4);
    if (ui->Button("Clear", 48, 24).activated) {
        netEntries.clear();
        networkRows.clear();
        ui->ClearListSelection("net_list");
    }

    if (networkRowsDirty) RebuildNetworkRows();

    int listY = py + TOOL_BAR_H;
    int listH = ph - TOOL_BAR_H;
    ui->SetCursor(px, listY);

    int NetworkItem = ui->GetListSelection("net_list");
    if (ui->BeginListBox("net_list", pw, listH, true, 22)) {
        for (int i = 0; i < (int) networkRows.size(); ++i) {
            ui->PushID("Net_Tab_" + std::to_string(i));
            ui->PushStyleColor(UIColorVar::Text, networkRows[i].textColor);

            auto rowStart = ui->GetCursor();
            ui->Selectable("badgeLabel", "", i == NetworkItem, pw - 80, 20, networkRows[i].textColor);
            // restore cursor for overlay content
            ui->SetCursor(rowStart.x + 6, rowStart.y + 2);

            ui->Text("Badge", networkRows[i].badgeLabel);
            ui->SameLine(10);
            ui->Text("Text", networkRows[i].text);
            ui->PopStyleColor();
            ui->NewLine();
            ui->PopID();
        }
        ui->EndListBox();
    }
}

static bool hasMultipleLines(const std::string &str) {
    // Returns true if a newline character is found
    return str.find('\n') != std::string::npos;
}

void DebugWindowManager::RenderInspectorTreeNode(const Node *node, int treeW) {
    if (!node) return;
    std::string name;
    std::string EndTag = "";
    bool RenderChildren = true;
    switch (node->type) {
        case (NodeType::Element): {
            name += "<" + node->tag;
            for (auto &attr: node->attributes) {
                name += " " + attr.first + "=\"" + attr.second + "\"";
            }
            name += ">";
            EndTag = "</" + node->tag + ">";
            // check if Text child
            if (node->children.size() == 1 && node->children[0]->type == NodeType::Text) {
                if (hasMultipleLines(node->children[0]->text) || node->children[0]->text.size() > 10) {
                    RenderChildren = true;
                    if (!ui->IsTreeOpen("Node")) {
                        name += "...";
                        name += EndTag;
                    }
                } else {
                    RenderChildren = false;
                    name += node->children[0]->text;
                    if (!ui->IsTreeOpen("Node")) {
                        name += EndTag;
                    }
                }
            }
            break;
        }
        case (NodeType::Comment): {
            name += "<!--" + node->text + "-->";
            break;
        }
        case (NodeType::Doctype): {
            name += "<!DOCTYPE " + node->text + ">";
            break;
        }
        case (NodeType::Image): {
            name += "<img />";
        }
        break;
        case NodeType::Document:
            break;
        case NodeType::Text:

            break;
    }

    if (node->type == NodeType::Text) {
        // has to go first

        auto split_view = node->text | std::views::split('\n');
        int i = 0;
        for (auto &&chunk: split_view) {
            std::string_view line(chunk.begin(), chunk.end());
            ui->Text("Text" + std::to_string(i), line.data());
            i++;
        }
        return;
    }
    if (node->children.empty() || !RenderChildren) {
        if (ui->Selectable("Node_sel", name, selectedNode == node, treeW - ui->GetCursor().x)) {
            selectedNode = node;
            redrawView = true;
            platform->needsRedraw = true;
            BuildStyleRows();
        }
        return;
    }

    bool clicked = false;
    if (ui->TreeNode("Node", name, clicked, selectedNode == node, treeW)) {
        for (int i = 0; i < (int) node->children.size(); ++i) {
            auto &child = node->children[i];
            ui->PushID(std::to_string(i));
            RenderInspectorTreeNode(child.get(), treeW);
            ui->PopID();
        }

        if (!EndTag.empty()) {
            if (ui->Selectable("EndTag", EndTag, false, treeW)) {
                selectedNode = node;
                platform->needsRedraw = true;
                redrawView = true;
                BuildStyleRows();
            }
        }
        ui->TreePop("Node");
    }
    if (clicked) {
        selectedNode = node;
        platform->needsRedraw = true;
        redrawView = true;
        BuildStyleRows();
    }
}

void DebugWindowManager::RenderInspectorTab(int px, int py, int pw, int ph) {
    if (inspectorDirty)
        RebuildInspectorRows();

    int treeW = (pw * 6) / 10;
    int stylesW = pw - treeW;
    int listH = ph;

    // -------------------------------------------------
    // DOM TREE
    // -------------------------------------------------
    ui->SetCursor(px, py);
    if (ui->BeginListBox("dom_tree", treeW, listH)) {
        ui->PushStyleColor(UIColorVar::Text, {235, 225, 255});
        for (int i = 0; i < (int) domRoot->children.size(); ++i) {
            ui->PushID(std::to_string(i));
            RenderInspectorTreeNode(domRoot->children[i].get(), treeW);
            ui->PopID();
        }
        ui->PopStyleColor();
        ui->EndListBox();
    }


    // -------------------------------------------------
    // STYLE PANE
    // -------------------------------------------------
    ui->SetCursor(px + treeW, py);

    if (ui->BeginListBox("style_pane", stylesW, listH, false, 20)) {
        for (int i = 0; i < (int) styleRows.size(); ++i) {
            auto &row = styleRows[i];
            auto Cursor = ui->GetCursor();
            ui->SetCursor(Cursor.x + 4, Cursor.y + 4);
            ui->BeginHorizontal(4);


            if (!row.badgeLabel.empty()) {
                ui->PushStyleColor(
                    UIColorVar::ButtonNormal,
                    row.badgeColor
                );

                ui->PushStyleColor(
                    UIColorVar::ButtonText,
                    row.badgeText
                );

                ui->Button(row.badgeLabel, 42, 18);

                ui->PopStyleColor(2);
            }

            ui->Label(row.text, row.textColor);

            ui->EndHorizontal();
            ui->SetCursor(Cursor.x, ui->GetCursor().y);
        }

        ui->EndListBox();
    }
}

// ---------------------------------------------------------------------------
// Main render
// ---------------------------------------------------------------------------
void DebugWindowManager::HandleEvent(const Event &event) {
    if (!platform || !ui) return;

    if (event.type == EventType::Resize) {
        windowWidth = event.width;
        windowHeight = event.height;

        ui->Resize(windowWidth, windowHeight);
        redrawRequested = true;
    } else if (event.type == EventType::KeyPress) {
        if (event.key == Key::LShift || event.key == Key::RShift)
            shiftHeld = true;

        if (event.key == Key::F12) {
            NeedsClose = true;
            return;
        }

        ui->InjectKeyChar(event.key, shiftHeld);
        redrawRequested = true;
    } else if (event.type == EventType::KeyRelease) {
        if (event.key == Key::LShift || event.key == Key::RShift)
            shiftHeld = false;
    } else if (event.type == EventType::MouseMove) {
        ui->InjectMouseMove(event.x, event.y);
        redrawRequested = true;
    } else if (event.type == EventType::MouseButtonPress &&
               event.button == 1) {
        ui->InjectMouseButton(true);
        redrawRequested = true;
    } else if (event.type == EventType::MouseButtonRelease &&
               event.button == 1) {
        ui->InjectMouseButton(false);
        redrawRequested = true;
    } else if (event.type == EventType::MouseWheel) {
        ui->InjectMouseWheel(event.WheelDelta);
        redrawRequested = true;
    }
}

bool DebugWindowManager::Render() {
    if (!isOpen || !platform || !ui)
        return false;


    while (true) {
        auto log = Logger::GetNextLog();

        if (!log.has_value())
            break;

        auto data = log.value();

        PushLog(
            data.level,
            data.msg,
            data.source,
            data.Indent
        );

        redrawRequested = true;
    }

    auto grabLog = CurlGrabber::GetGrabLog();


    for (auto &log: grabLog) {
        netEntries.push_back(log.netDebug);
    }
    CurlGrabber::ResetLog();

    SetNetworkEntries(netEntries);
    redrawRequested = false;

    ui->BeginFrame();

    const int W = windowWidth;
    const int H = windowHeight;

    // === Tab bar ===
    ui->SetCursor(4, 4);
    for (int i = 0; i < 3; ++i) {
        const char *tabNames[] = {"Console", "Network", "Inspector"};
        std::string tabTitle = tabNames[i];
        if (ui->Tab(std::string("dbtab_") + tabNames[i], tabTitle, activeTab == i, 90, 26)) {
            activeTab = i;
        }
        ui->SameLine(2);
    }

    auto emptySpace = ui->GetCursor();
    windowWidth = platform->GetWidth();
    auto saved = ui->GetCursor();

    ui->SetCursor(windowWidth - 65, saved.y);

    if (ui->SvgButton("minimize", minimize, 28, 28).activated) {
        platform->MinimizeWindow();
    }
    ui->SameLine(0);

    if (ui->SvgButton("minmax", platform->Is_WindowZoomed() ? Return : maximize, 28, 28).activated) {
        platform->MaximizeOrRestoreWindow();
    }
    ui->SameLine(0);

    ui->SetCursor(saved.x, saved.y);
    platform->SetTopBarHeight({emptySpace.x, 0, platform->GetWidth() - emptySpace.x - 89, 28});

    // === Content area below tab bar ===
    int contentY = TAB_BAR_H + 4;
    int contentH = H - contentY;

    switch (activeTab) {
        case 0: RenderConsoleTab(0, contentY, W, contentH);
            break;
        case 1: RenderNetworkTab(0, contentY, W, contentH);
            break;
        case 2: RenderInspectorTab(0, contentY, W, contentH);
            break;
        default: break;
    }

    ui->EndFrame();


    return !NeedsClose;
}

const Node *DebugWindowManager::GetSelectedNode() const {
    if (activeTab == 2) {
        return selectedNode;
    }
    return nullptr;
}

bool DebugWindowManager::Redraw() {
    if (redrawView) {
        redrawView = false;
        return true;
    }
    return false;
}
