#include "DebugWindowManager.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

// Your project's Platform factory and Node definition
#include <source_location>

#include "CurlGrabber.h"
#include "Logger.h" // Updated include path matching your header guard
#include "Platform/Platform.h"
#include "Node.h"   // adjust path to wherever Node lives

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Color LevelBadgeBg(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Fatal:   return {130, 10,  10 };   // Added for Fatal
        case LogLevel::Error:   return {160, 40,  40 };
        case LogLevel::Warning: return {160, 120, 20 };   // Fixed mapping name
        case LogLevel::Info:    return {30,  90,  160};
        case LogLevel::Debug:   return {100, 40,  160};   // Added for Debug
        case LogLevel::None:
        case LogLevel::Verbose:
        default:                return {60,  62,  65 };
    }
}

static std::string LevelBadgeStr(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Fatal:   return "FTL";             // Added for Fatal
        case LogLevel::Error:   return "ERR";
        case LogLevel::Warning: return "WRN";             // Fixed mapping name
        case LogLevel::Info:    return "INF";
        case LogLevel::Debug:   return "DBG";             // Added for Debug
        case LogLevel::Verbose: return "VRB";
        case LogLevel::None:
        default:                return "LOG";
    }
}

static Color LevelRowTint(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Fatal:   return {90,  10, 10};     // Added for Fatal
        case LogLevel::Error:   return {80,  20, 20};
        case LogLevel::Warning: return {70,  55, 10};     // Fixed mapping name
        default:                return {0,   0,  0 };
    }
}

static bool LevelHasTint(LogLevel lvl) {
    return (lvl == LogLevel::Fatal || lvl == LogLevel::Error || lvl == LogLevel::Warning);
}

static std::string StatusStr(int code) {
    if (code == 0) return "—";
    return std::to_string(code);
}

static Color StatusBadgeBg(int code) {
    if (code == 0)          return {70,  70,  70 };   // pending
    if (code >= 500)        return {160, 40,  40 };   // server error
    if (code >= 400)        return {160, 80,  20 };   // client error
    if (code >= 300)        return {100, 100, 20 };   // redirect
    return {30, 110, 50};                             // 2xx ok
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

DebugWindowManager::DebugWindowManager(int width, int height)
    : windowWidth(width), windowHeight(height)
{
}

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------

bool DebugWindowManager::Open() {
    if (isOpen) return true;

    platform = CreatePlatform();
    if (!platform->OpenWindow(windowWidth, windowHeight, "DevTools")) {
        platform.reset();
        return false;
    }
    platform->SetMinimumSize(600, 380);

    ui = std::make_unique<DebugInterfaceManager>(windowWidth, windowHeight);

    // Mark all data dirty so first frame fully rebuilds everything
    consoleRowsDirty  = true;
    networkRowsDirty  = true;
    inspectorDirty    = true;

    isOpen = true;
    return true;
}

void DebugWindowManager::Close() {
    if (!isOpen) return;
    platform.reset();
    ui.reset();
    isOpen = false;
}

// ---------------------------------------------------------------------------
// Data feeds
// ---------------------------------------------------------------------------

void DebugWindowManager::FeedDOM(const Node* root) {
    if (domRoot == root) return;
    domRoot        = root;
    selectedNode   = nullptr;
    styleRows.clear();
    inspectorDirty = true;
}

void DebugWindowManager::FeedJS(JavaScriptEngine *engine) {
    jsEngine = engine;
}

void DebugWindowManager::PushLog(LogLevel level,
                                 const std::string &message,
                                 const std::string &source, int indent)
{
    logEntries.push_back({level, message, source, indent});
    consoleRowsDirty = true;
}

void DebugWindowManager::ClearLogs() {
    logEntries.clear();
    consoleRows.clear();
    consoleRowsDirty = false;
    if (ui) ui->ClearListSelection("console_list");
}

void DebugWindowManager::SetNetworkEntries(const std::vector<DebugNetEntry>& entries) {
    netEntries       = entries;
    networkRowsDirty = true;
}

// ---------------------------------------------------------------------------
// Row builders
// ---------------------------------------------------------------------------

void DebugWindowManager::RebuildConsoleRows() {
    consoleRows.clear();
    for (const auto& e : logEntries) {
        // Filter by level toggles (Updated filter logic for new types)
        if (e.level == LogLevel::Verbose && !showLogs)   continue;
        if (e.level == LogLevel::Debug   && !showLogs)   continue;
        if (e.level == LogLevel::Info    && !showLogs)   continue;
        if (e.level == LogLevel::Warning && !showWarns)  continue;
        if (e.level == LogLevel::Error   && !showErrors) continue;
        if (e.level == LogLevel::Fatal   && !showErrors) continue;

        // Filter by text search
        if (!consoleFilter.empty()) {
            std::string lower = e.message;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            std::string filterLower = consoleFilter;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
            if (lower.find(filterLower) == std::string::npos) continue;
        }

        ListRow row;
        row.text        = e.message;
        row.annotation  = e.source;
        row.badgeLabel  = LevelBadgeStr(e.level);
        row.badgeColor  = LevelBadgeBg(e.level);
        row.badgeText   = {220, 222, 225};
        row.rowTint     = LevelRowTint(e.level);
        row.hasTint     = LevelHasTint(e.level);
        consoleRows.push_back(std::move(row));
    }
    consoleRowsDirty = false;
}

void DebugWindowManager::RebuildNetworkRows() {
    networkRows.clear();
    for (const auto& e : netEntries) {
        ListRow row;
        // Method badge
        row.badgeLabel  = e.method.empty() ? "GET" : e.method;
        row.badgeColor  = {45, 70, 110};
        row.badgeText   = {180, 210, 255};

        row.text        = e.url;
        row.annotation  = StatusStr(e.statusCode);
        row.badgeColor  = StatusBadgeBg(e.statusCode); // override with status colour
        row.badgeLabel  = StatusStr(e.statusCode);
        row.badgeText   = {220, 222, 225};

        // Separate annotation: show content type + size
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

        row.hasTint    = (e.statusCode >= 400 || e.statusCode == 0);
        row.rowTint    = (e.statusCode >= 500) ? Color{80, 20, 20}
                       : (e.statusCode >= 400) ? Color{80, 50, 10}
                       :                         Color{0,  0,  0};
        networkRows.push_back(std::move(row));
    }
    networkRowsDirty = false;
}

// ---------------------------------------------------------------------------
// Inspector: DOM tree flattening
// ---------------------------------------------------------------------------

ListRow DebugWindowManager::MakeNodeRow(const Node* node, int depth, bool &ChildrenHandled) const {
    ListRow row;

    ChildrenHandled = false;
    if (node->type == NodeType::Text) {
        std::string txt = node->text;
        // Collapse whitespace
        auto start = txt.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) { row.text =  "(whitespace)"; }
        else {
            txt = txt.substr(start, 60);
            row.text =  "\"" + txt + "\"";
        }
        row.badgeLabel = "TXT";
        row.badgeColor = {50, 50, 55};
        row.badgeText  = {140, 142, 145};
        row.IndentLevel = depth;
    } else if (node->type == NodeType::Comment) {
        std::string comment = "<!--" + node->text + "-->";
        row.text       = comment;
        row.badgeLabel = "CMT";
        row.badgeColor = {50, 50, 55};
        row.badgeText  = {140, 142, 145};
        row.IndentLevel = depth;
        row.TextColor = {100, 200, 100};
    } else if (node->type == NodeType::Doctype) {
        std::string doc = "<!DOCTYPE " + node->tag + ">";
        row.text       = doc;
        row.badgeLabel = "DOC";
        row.badgeColor = {50, 50, 55};
    }
    else {
        // Build  <tag#id.class>
        std::string label = "<" + node->tag;
        for (const auto& attr : node->attributes) {
            if (attr.first != "style") {
                label += " " + attr.first + "=\"" + attr.second + "\"";
            }
        }
        label += ">";

        if (node->children.size() == 1 && node->children[0]->type == NodeType::Text) {
            if (node->children[0]->text.find('\n') == std::string::npos || node->children[0]->text.size() < 20) {
                label += node->children[0]->text;
                ChildrenHandled = true;
            }
        } else if (node->children.size() > 1) {
            label += "...";
        }
        label += "</" + node->tag + ">";
        row.text       = label;
        row.badgeLabel = node->tag.empty() ? "?" : node->tag.substr(0, 4);
        row.badgeColor = {40, 60, 100};
        row.badgeText  = {160, 200, 255};
        row.IndentLevel = depth;
        if (node->tag == "body") {
            row.isCollapsed = false;
        }
    }

    return row;
}

void DebugWindowManager::FlattenDOM(const Node* node, int depth, bool Root = false) {
    if (!node) return;
    if (Root) { // skip the root node


        for (const auto& child : node->children) {
            FlattenDOM(child.get(), 0, false);
        }
        return;
    }
    bool ChildrenHandled = false;
    inspectorRows.push_back(MakeNodeRow(node, depth, ChildrenHandled));
    inspectorNodes.push_back(node);
    if (!ChildrenHandled) {
        for (const auto& child : node->children) {
            FlattenDOM(child.get(), depth + 1, false);
        }
    }

}

void DebugWindowManager::RebuildInspectorRows() {
    inspectorRows.clear();
    inspectorNodes.clear();
    FlattenDOM(domRoot, 0, true);
    inspectorDirty = false;
}

void DebugWindowManager::BuildStyleRows() {
    styleRows.clear();
    if (!selectedNode) return;

    // --- Enum and Type Stringifiers ---
    auto DisplayStr = [](DisplayType d) -> std::string {
        switch(d) {
            case DisplayType::Block:       return "block";
            case DisplayType::Inline:      return "inline";
            case DisplayType::InlineBlock: return "inline-block";
            case DisplayType::None:        return "none";
            default:                       return "unknown";
        }
    };

    auto PositionStr = [](PositionType p) -> std::string {
        switch(p) {
            case PositionType::Static:   return "static";
            case PositionType::Relative: return "relative";
            case PositionType::Absolute: return "absolute";
            default:                     return "unknown";
        }
    };

    auto OverflowStr = [](OverflowType o) -> std::string {
        switch(o) {
            case OverflowType::Visible: return "visible";
            case OverflowType::Hidden:  return "hidden";
            case OverflowType::Scroll:  return "scroll";
            default:                    return "unknown";
        }
    };

    auto ColorStr = [](const Color& c) -> std::string {
        return "rgb(" + std::to_string(c.r) + ", " + std::to_string(c.g) + ", " + std::to_string(c.b) + ")";
    };

    auto LengthStr = [](const CSSLength& len) -> std::string {
        if (len.unit == LengthUnit::Auto) return "auto";
        if (len.unit == LengthUnit::Inherit) return "inherit";

        std::string valStr = std::to_string(len.value);
        valStr.erase(valStr.find_last_not_of('0') + 1, std::string::npos);
        if (valStr.back() == '.') valStr.pop_back();

        switch (len.unit) {
            case LengthUnit::Px:      return valStr + "px";
            case LengthUnit::Percent: return valStr + "%";
            case LengthUnit::Em:      return valStr + "em";
            default:                  return valStr;
        }
    };

    auto addRow = [&](const std::string& prop, const std::string& val) {
        if (val.empty() || val == "0" || val == "0px" || val == "none") return;
        ListRow r;
        r.text = prop + ": " + val;
        styleRows.push_back(r);
    };

    // --- Computed Styles Section ---
    const Style& cs = selectedNode->computedStyle;

    ListRow header;
    header.text       = "computed style";
    header.badgeLabel = "CSS";
    header.badgeColor = {60, 30, 100};
    header.badgeText  = {200, 170, 255};
    styleRows.push_back(header);

    addRow("display",    DisplayStr(cs.display));
    addRow("color",      ColorStr(cs.color));
    addRow("background", cs.hasBackground ? ColorStr(cs.backgroundColor) : "none");
    addRow("font-size",  LengthStr(cs.font_size));
    addRow("font-weight",cs.font_bold ? "bold" : "normal");
    addRow("width",      LengthStr(cs.width));
    addRow("height",     LengthStr(cs.height));
    addRow("margin-top", LengthStr(cs.margin_top));
    addRow("margin-left",LengthStr(cs.margin_left));
    addRow("padding-top",LengthStr(cs.padding_top));
    addRow("position",   PositionStr(cs.position));
    addRow("overflow",   OverflowStr(cs.overflow));

    // --- Specified Styles Section ---
    const Style& ss = selectedNode->specifiedStyle;

    ListRow specHeader;
    specHeader.text       = "specified style";
    specHeader.badgeLabel = "CSS";
    specHeader.badgeColor = {30, 70, 60};
    specHeader.badgeText  = {160, 220, 200};
    styleRows.push_back(specHeader);

    addRow("display",    DisplayStr(ss.display));
    addRow("color",      ColorStr(ss.color));
    addRow("background", ss.hasBackground ? ColorStr(ss.backgroundColor) : "none");
    addRow("font-size",  LengthStr(ss.font_size));
    addRow("font-weight",ss.font_bold ? "bold" : "normal");
    addRow("width",      LengthStr(ss.width));
    addRow("height",     LengthStr(ss.height));
    addRow("margin-top", LengthStr(ss.margin_top));
    addRow("margin-left",LengthStr(ss.margin_left));
    addRow("padding-top",LengthStr(ss.padding_top));
    addRow("position",   PositionStr(ss.position));
}

// ---------------------------------------------------------------------------
// Per-tab renderers
// ---------------------------------------------------------------------------

void DebugWindowManager::RenderConsoleTab(int px, int py, int pw, int ph) {
    ui->SetCursor(px + 4, py + 4);

    bool filterChanged = false;
    if (ui->TextField("console_filter", consoleFilter, pw - 220, 24)) {
        filterChanged = true;
    }
    ui->SameLine(8);

    bool prevLogs   = showLogs;
    bool prevWarns  = showWarns;
    bool prevErrors = showErrors;

    ui->Checkbox("cb_log",  "Log",  showLogs);   ui->SameLine(8);
    ui->Checkbox("cb_warn", "Warn", showWarns);  ui->SameLine(8);
    ui->Checkbox("cb_err",  "Err",  showErrors);

    if (showLogs != prevLogs || showWarns != prevWarns || showErrors != prevErrors || filterChanged) {
        consoleRowsDirty = true;
    }

    ui->NewLine(4);

    ui->SetCursor(px + pw - 52, py + 4);
    if (ui->Button("Clear", 48, 24)) {
        ClearLogs();
        platform->needsRedraw = true;
    }

    // --- Layout Math adjustments for JS input row ---
    int jsInputH  = 32; // Height reserved for the bottom JS tray
    int listY     = py + TOOL_BAR_H;
    int listH     = ph - TOOL_BAR_H - jsInputH;

    if (consoleRowsDirty) {
        platform->needsRedraw = true;
        RebuildConsoleRows();
        if (consoleAutoScroll) {
            ui->ScrollListToBottom("console_list", consoleRows, listH, 22);
        }
    }

    // Render the log list with compressed height
    ui->SetCursor(px, listY);
    (void)ui->ScrollableList("console_list", consoleRows, pw, listH, true, 22);

    // --- Bottom JS Console Bar ---
    int jsY = py + ph - jsInputH + 4;
    ui->SetCursor(px + 4, jsY);

    int buttonW = 50;
    int inputW  = pw - buttonW - 16;

    // Optional: Render a small label or prompt symbol
    // ui->Label(">", 10, 24);

    // Text field tracking code input
    bool jsSubmitted = ui->TextField("js_input_bar", jsBuffer, inputW, 24);
    ui->SameLine(8);

    // Evaluate if user presses Enter inside the text field or clicks 'Run'
    if (jsSubmitted || ui->Button("Run", buttonW, 24)) {
        if (!jsBuffer.empty()) {
            if (jsEngine) {
                // Log the command visually in the console
                Logger::Log("> " + jsBuffer, "Console Input", 0);

                // Execute code via your registered engine interface
                // (Adapt the exact evaluation method name to your JavaScriptEngine API)
                auto result = jsEngine->Run(jsBuffer, "Console.js");
                Logger::Log(result, "Console Output", 0);
            } else {
                Logger::Log_Warning( "JS Engine not attached.", "Debugger", 0);
            }

            jsBuffer.clear(); // Clear input bar after evaluation
            consoleRowsDirty = true;
            platform->needsRedraw = true;
        }
    }
}
void DebugWindowManager::RenderNetworkTab(int px, int py, int pw, int ph) {
    ui->SetCursor(px + 4, py + 4);

    ui->SetCursor(px + pw - 52, py + 4);
    if (ui->Button("Clear", 48, 24)) {
        netEntries.clear();
        networkRows.clear();
        ui->ClearListSelection("net_list");
    }

    if (networkRowsDirty) RebuildNetworkRows();

    int listY = py + TOOL_BAR_H;
    int listH = ph - TOOL_BAR_H;
    ui->SetCursor(px, listY);

    ui->ScrollableList("net_list", networkRows, pw, listH, false, 22);
}

void DebugWindowManager::RenderInspectorTab(int px, int py, int pw, int ph) {
    if (inspectorDirty) RebuildInspectorRows();

    int treeW   = (pw * 6) / 10;
    int stylesW = pw - treeW;
    int listH   = ph;

    ui->SetCursor(px, py);
    int clickedNode = ui->ScrollableList("dom_tree", inspectorRows, treeW, listH, false, 20);

    if (clickedNode >= 0 && clickedNode < (int)inspectorNodes.size()) {
        const Node* node = inspectorNodes[clickedNode];
        if (node != selectedNode) {
            selectedNode = node;
            BuildStyleRows();
        }
    }

    ui->SetCursor(px + treeW, py);
    ui->ScrollableList("style_pane", styleRows, stylesW, listH, false,20);
}

// ---------------------------------------------------------------------------
// Main render
// ---------------------------------------------------------------------------

bool DebugWindowManager::Render() {
    if (!isOpen || !platform || !ui) return false;

    // Assuming you have access to a Logger instance parameter or pointer
    // If you plan to use a global object, exchange `loggerInstance.` with your pointer name.
    while (true) {
        auto log = Logger::GetNextLog();
        if (!log.has_value()) {
            break;
        }
        auto data = log.value();
        // Passing "" as source because the new LogEntry layout doesn't track a source parameter natively
        PushLog(data.level, data.msg, data.source, data.Indent);
        platform->needsRedraw = true;
    }

    auto GrabLog = CurlGrabber::GetGrabLog();
    std::vector<DebugNetEntry> netEntries;
        for (auto log : GrabLog) {
            netEntries.push_back(log.netDebug);
        }
    SetNetworkEntries(netEntries);
    // --- Event pump for the debug window ---
    Event event;
    while (platform->PollEvent(event)) {
        if (event.type == EventType::Resize) {
            windowWidth  = event.width;
            windowHeight = event.height;
            ui->Resize(windowWidth, windowHeight);
            platform->needsRedraw = true;
        }
        else if (event.type == EventType::KeyPress) {
            if (event.key == Key::LShift || event.key == Key::RShift) shiftHeld = true;
            ui->InjectKeyChar(event.key, shiftHeld);
            platform->needsRedraw = true;
        }
        else if (event.type == EventType::KeyRelease) {
            if (event.key == Key::LShift || event.key == Key::RShift) shiftHeld = false;
        }
        else if (event.type == EventType::MouseMove) {
            ui->InjectMouseMove(event.x, event.y);
            platform->needsRedraw = true;
        }
        else if (event.type == EventType::MouseButtonPress && event.button == 1) {
            ui->InjectMouseButton(true);
            platform->needsRedraw = true;
        }
        else if (event.type == EventType::MouseButtonRelease && event.button == 1) {
            ui->InjectMouseButton(false);
            platform->needsRedraw = true;
        }
        else if (event.type == EventType::MouseWheel) {
            ui->InjectMouseWheel(event.WheelDelta);
            platform->needsRedraw = true;
        }
        else if (event.type == EventType::Quit) {
            Close();
            return false;
        }
    }

    if (!platform->IsRunning()) {
        Close();
        return false;
    }

    if (!platform->needsRedraw) return true;
    platform->needsRedraw = false;

    // --- Build frame ---
    ui->BeginFrame();

    const int W = windowWidth;
    const int H = windowHeight;

    // === Tab bar ===
    ui->SetCursor(4, 4);
    const char* tabNames[] = { "Console", "Network", "Inspector" };
    for (int i = 0; i < 3; ++i) {
        std::string tabTitle = tabNames[i];
        if (ui->Tab(std::string("dbtab_") + tabNames[i], tabTitle, activeTab == i, 90, 26)) {
            activeTab = i;
        }
        ui->SameLine(2);
    }

    // === Content area below tab bar ===
    int contentY = TAB_BAR_H + 4;
    int contentH = H - contentY;

    switch (activeTab) {
        case 0: RenderConsoleTab (0, contentY, W, contentH); break;
        case 1: RenderNetworkTab (0, contentY, W, contentH); break;
        case 2: RenderInspectorTab(0, contentY, W, contentH); break;
    }

    ui->EndFrame();

    platform->Present(ui->GetFrontBuffer());
    return true;
}