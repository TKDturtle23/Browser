#include "DebugWindowManager.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

// Your project's Platform factory and Node definition
#include <ranges>
#include <source_location>

#include "CurlGrabber.h"
#include "../Debug/Logger.h"
#include "Platform/Platform.h"
#include "Node.h"   // adjust path to wherever Node lives

// ---------------------------------------------------------------------------
// Helpers (Light Mode Palette)
// ---------------------------------------------------------------------------

static Color LevelBadgeBg(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Fatal:   return {255, 215, 215};   // Light Red
        case LogLevel::Error:   return {255, 225, 225};   // Soft Red
        case LogLevel::Warning: return {255, 243, 205};   // Soft Amber/Yellow
        case LogLevel::Info:    return {207, 244, 252};   // Soft Light Blue
        case LogLevel::Debug:   return {243, 230, 255};   // Soft Lavender/Purple
        case LogLevel::None:
        case LogLevel::Verbose:
        default:                return {240, 242, 245};   // Light Cool Gray
    }
}

static std::string LevelBadgeStr(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Fatal:   return "FTL";
        case LogLevel::Error:   return "ERR";
        case LogLevel::Warning: return "WRN";
        case LogLevel::Info:    return "INF";
        case LogLevel::Debug:   return "DBG";
        case LogLevel::Verbose: return "VRB";
        case LogLevel::None:
        default:                return "LOG";
    }
}

// Consistent Dark Text for Light Mode Badges
static Color LevelBadgeText(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Fatal:   return {130, 10,  10 };   // Dark Red
        case LogLevel::Error:   return {160, 40,  40 };   // Dark Red
        case LogLevel::Warning: return {133, 100, 4  };   // Dark Brown/Yellow
        case LogLevel::Info:    return {11,  87,  208};   // Dark Blue
        case LogLevel::Debug:   return {100, 40,  160};   // Dark Purple
        default:                return {60,  62,  65 };   // Dark Gray
    }
}

static Color LevelRowTint(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Fatal:   return {255, 235, 235};   // Very faint red background
        case LogLevel::Error:   return {255, 240, 240};   // Faint red background
        case LogLevel::Warning: return {255, 250, 230};   // Faint yellow background
        default:                return {255, 255, 255};   // Pure white fallback
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
    if (code == 0)          return {230, 230, 230};   // Pending (Neutral Light Gray)
    if (code >= 500)        return {255, 225, 225};   // Server Error (Soft Red)
    if (code >= 400)        return {255, 243, 205};   // Client Error (Soft Amber)
    if (code >= 300)        return {230, 245, 230};   // Redirect (Faint Green)
    return {212, 242, 218};                           // 2xx OK (Soft Green)
}

static Color StatusBadgeText(int code) {
    if (code == 0)          return {80,  80,  80 };
    if (code >= 500)        return {160, 40,  40 };
    if (code >= 400)        return {133, 100, 4  };
    if (code >= 300)        return {30,  90,  30 };
    return {20,  100, 40 };
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

DebugWindowManager::DebugWindowManager(int width, int height)
    : windowWidth(width), windowHeight(height)
{
minimize = ui->MakeImage("./Assets/Icons/minus.svg", 20, 20);
maximize = ui->MakeImage("./Assets/Icons/maximize.svg", 20, 20);
Return    = ui->MakeImage("./Assets/Icons/minimize.svg",    20, 20);
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
        if (e.level == LogLevel::Verbose && !showLogs)   continue;
        if (e.level == LogLevel::Debug   && !showLogs)   continue;
        if (e.level == LogLevel::Info    && !showLogs)   continue;
        if (e.level == LogLevel::Warning && !showWarns)  continue;
        if (e.level == LogLevel::Error   && !showErrors) continue;
        if (e.level == LogLevel::Fatal   && !showErrors) continue;

        if (!consoleFilter.empty()) {
            std::string lower = e.message;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            std::string filterLower = consoleFilter;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
            if (lower.find(filterLower) == std::string::npos) continue;
        }

        DebugListRow row;
        row.text        = e.message;
        row.annotation  = e.source;
        row.badgeLabel  = LevelBadgeStr(e.level);
        row.badgeColor  = LevelBadgeBg(e.level);
        row.badgeText   = LevelBadgeText(e.level);
        row.rowTint     = LevelRowTint(e.level);
        row.hasTint     = LevelHasTint(e.level);
        row.textColor   = {30, 30, 30}; // Dark text for list rows
        consoleRows.push_back(std::move(row));
    }
    consoleRowsDirty = false;

}

void DebugWindowManager::RebuildNetworkRows() {
    networkRows.clear();
    for (const auto& e : netEntries) {
        DebugListRow row;

        row.badgeLabel  = e.method.empty() ? "GET" : e.method;
        row.badgeColor  = {230, 240, 255}; // Faint light blue for methods
        row.badgeText   = {30, 80, 180};    // High-contrast blue text

        row.text        = e.url;
        row.annotation  = StatusStr(e.statusCode);
        row.badgeColor  = StatusBadgeBg(e.statusCode);
        row.badgeLabel  = StatusStr(e.statusCode);
        row.badgeText   = StatusBadgeText(e.statusCode);
        row.textColor   = {30, 30, 30};

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

        row.hasTint    = true;
        row.rowTint    = (e.statusCode >= 500) ? Color{255, 235, 235}
                       : (e.statusCode >= 400) ? Color{255, 250, 230}
                       :                         Color{255, 255, 255};
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
            case LengthUnit::Vh:      return valStr + "vh";
            case LengthUnit::Vw:      return valStr + "vw";
            default:                  return valStr;
        }
    };

    auto addRow = [&](const std::string& prop, const std::string& val) {
        if (val.empty() || val == "0" || val == "0px" || val == "none") return;
        DebugListRow r;
        r.text = prop + ": " + val;
        r.textColor = {50, 50, 50};
        styleRows.push_back(r);
    };

    // --- Computed Styles Section ---
    const Style& cs = selectedNode->computedStyle;

    DebugListRow header;
    header.text       = "computed style";
    header.badgeLabel = "CSS";
    header.badgeColor = {243, 230, 255}; // Faint Purple
    header.badgeText  = {100, 40, 160};   // Dark Purple Text
    header.textColor  = {30, 30, 30};
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

    DebugListRow specHeader;
    specHeader.text       = "specified style";
    specHeader.badgeLabel = "CSS";
    specHeader.badgeColor = {210, 245, 240}; // Faint Teal
    specHeader.badgeText  = {10, 100, 90};    // Dark Teal Text
    specHeader.textColor  = {30, 30, 30};
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
    if (ui->Button("Clear", 48, 24).activated) {
        ClearLogs();
        platform->needsRedraw = true;
    }

    int jsInputH  = 32;
    int listY     = py + TOOL_BAR_H;
    int listH     = ph - TOOL_BAR_H - jsInputH;

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

        for (int i = 0; i < (int)consoleRows.size(); ++i) {
            ui->PushID("Console_Tab_" + std::to_string(i));

            auto rowStart = ui->GetCursor();

            // background/selectable FIRST
            bool clicked = ui->Selectable(
                "Selector", "",
                i == SelectedItem,
                pw,
                20,
                Color(0,0,0,0)
            );

            // restore cursor for overlay content
            ui->SetCursor(rowStart.x + 6, rowStart.y + 2);

            ui->Text("Badge", consoleRows[i].badgeLabel);
            ui->SameLine(10);
            ui->Text("Text", consoleRows[i].text);

            ui->SetCursor(px + pw - ui->MeasureText(consoleRows[i].annotation) - 10, rowStart.y + 2);
            ui->PushStyleColor(UIColorVar::Text, Color(80,  80,  80,  255));
            ui->Text("Annotation", consoleRows[i].annotation);

            ui->NewLine();

            ui->PopID();
        }

        ui->EndListBox();
    }

    int jsY = py + ph - jsInputH + 4;
    ui->SetCursor(px + 4, jsY);

    int buttonW = 50;
    int inputW  = pw - buttonW - 16;

    WidgetResult jsSubmitted = ui->TextField("js_input_bar", jsBuffer, inputW, 24);
    ui->SameLine(8);

    if (jsSubmitted || ui->Button("Run", buttonW, 24).activated) {
        if (!jsBuffer.empty()) {
            if (jsEngine) {
                Logger::Log("> " + jsBuffer, "Console Input", 0);
                auto result = jsEngine->Run(jsBuffer, "Console.js");
                Logger::Log(result, "Console Output", 0);

            } else {
                Logger::Log_Warning( "JS Engine not attached.", "Debugger", 0);
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
        for (int i = 0; i < (int)networkRows.size(); ++i) {
            ui->PushID("Net_Tab_" + std::to_string(i));
            auto rowStart = ui->GetCursor();
            ui->Selectable("badgeLabel", "",  i == NetworkItem, pw - 80, 20, networkRows[i].textColor);
            // restore cursor for overlay content
            ui->SetCursor(rowStart.x + 6, rowStart.y + 2);

            ui->Text("Badge", networkRows[i].badgeLabel);
            ui->SameLine(10);
            ui->Text("Text", networkRows[i].text);

            ui->NewLine();
            ui->PopID();
        }
        ui->EndListBox();
    }
}
static bool hasMultipleLines(const std::string& str) {
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
            for (auto& attr : node->attributes) {
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
        } break;
        case NodeType::Document:
            break;
        case NodeType::Text:

            break;
    }
    if (node->type == NodeType::Text) { // has to go first
        auto split_view = node->text | std::views::split('\n');
        int i = 0;
        for (auto&& chunk : split_view) {
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

        for (int i = 0; i < (int)node->children.size(); ++i) {
            auto& child = node->children[i];
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

void DebugWindowManager::RenderInspectorTab(int px, int py, int pw, int ph)
{
    if (inspectorDirty)
        RebuildInspectorRows();

    int treeW   = (pw * 6) / 10;
    int stylesW = pw - treeW;
    int listH   = ph;

    // -------------------------------------------------
    // DOM TREE
    // -------------------------------------------------
    ui->SetCursor(px, py);
    if (ui->BeginListBox("dom_tree", treeW, listH)) {
        for (int i = 0; i < (int)domRoot->children.size(); ++i) {
            ui->PushID(std::to_string(i));
            RenderInspectorTreeNode(domRoot->children[i].get(), treeW);
            ui->PopID();
        }
    ui->EndListBox();
    }


    // -------------------------------------------------
    // STYLE PANE
    // -------------------------------------------------
    ui->SetCursor(px + treeW, py);

    if (ui->BeginListBox("style_pane", stylesW, listH, false, 20)) {

        for (int i = 0; i < (int)styleRows.size(); ++i) {

            auto& row = styleRows[i];
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

bool DebugWindowManager::Render() {
    if (!isOpen || !platform || !ui) return false;

    while (true) {
        auto log = Logger::GetNextLog();
        if (!log.has_value()) {
            break;
        }
        auto data = log.value();
        PushLog(data.level, data.msg, data.source, data.Indent);
        platform->needsRedraw = true;
    }

    auto GrabLog = CurlGrabber::GetGrabLog();
    std::vector<DebugNetEntry> netEntries;
    for (auto log : GrabLog) {
        netEntries.push_back(log.netDebug);
    }
    SetNetworkEntries(netEntries);

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
            if (event.key == Key::F12) {
                NeedsClose = true;
                return false;
            }
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
            NeedsClose = true;
            return false;
        }
    }

    if (!platform->IsRunning()) {
        Close();
        return false;
    }

    if (!platform->needsRedraw) return true;
    platform->needsRedraw = false;

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

    auto emptySpace = ui->GetCursor();
    int windowWidth = platform->GetWidth();
    auto saved = ui->GetCursor();

    ui->SetCursor(windowWidth - 65, saved.y);

    if (ui->SvgButton("minimize", minimize,28, 28).activated) {
        platform->MinimizeWindow();
    }
    ui->SameLine(0);

    if (ui->SvgButton("minmax", platform->Is_WindowZoomed() ? Return : maximize,28, 28).activated) {
        platform->MaximizeOrRestoreWindow();
    }
    ui->SameLine(0);

    ui->SetCursor(saved.x, saved.y);
    platform->SetTopBarHeight({emptySpace.x, 0, platform->GetWidth() - emptySpace.x - 89, 28});

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

const Node *DebugWindowManager::GetSelectedNode() {
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