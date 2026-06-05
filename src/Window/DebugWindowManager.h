#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "UI/InterfaceManager.h"
#include "Render/Backend/IRendererBackend.h"
#include "../Debug/Logger.h"
#include "JavaScriptEngine/JavaScriptEngine.h"
#include "CurlGrabber.h"
// Forward declarations
class Platform;
struct Node;
struct Color;

// ---------------------------------------------------------------------------
// Console log entry — push one of these from anywhere in the browser
// ---------------------------------------------------------------------------

struct DebugLogEntry {
    LogLevel level  = LogLevel::Info;
    std::string   message;
    std::string   source;   // e.g. "dom.js:42"
    int           indent   = 0;
};

struct DebugListRow {
    std::string text;
    std::string annotation;
    std::string badgeLabel;

    Color badgeColor  = {60,60,60,255};
    Color badgeText   = {255,255,255,255};

    bool  hasTint     = false;
    Color rowTint     = {0,0,0,0};

    bool  isCollapsed = false;
    int   indentLevel = 0;

    Color textColor   = {220,220,220,255};
};
// ---------------------------------------------------------------------------
// DebugWindowManager
//
// Owns the F12 debug OS window. Feed it data every frame from WindowManager.
// Call Open() on F12, Close() to hide, IsOpen() to guard rendering.
// ---------------------------------------------------------------------------
class DebugWindowManager {
public:
    DebugWindowManager(int width = 900, int height = 500);
    ~DebugWindowManager() = default;

    // --- Lifetime -----------------------------------------------------------
    // Opens (or re-opens) the debug OS window.
    bool Open();

    // Closes/hides the debug window without destroying internal state.
    void Close();

    bool IsOpen() const { return isOpen; }

    // --- Data feeds (call every frame before Render, cheap if not open) -----

    // Full DOM tree of the currently active tab. Pass nullptr to clear.
    void FeedDOM(const Node* root);
    void FeedJS(JavaScriptEngine *engine);

    // Append a console log line. Duplicate-frame-safe.
    void PushLog(LogLevel level,
                                 const std::string &message,
                                 const std::string &source, int indent);

    void ClearLogs();
    Platform* GetPlatform() const;
    // Replace the full network table (simplest; rebuild from your resource loader).
    void SetNetworkEntries(const std::vector<DebugNetEntry>& entries);

    // --- Per-frame call (call inside WindowManager::Run after main render) --
    // Pumps the debug window's own event loop and renders one frame.
    // Returns false if the window was closed by the user (so caller can set isOpen = false).
    bool Render();

    const Node *GetSelectedNode();
    bool Redraw();

    void HandleEvent(const Event& event);
    bool NeedsClosing() { return NeedsClose;}
    bool NeedsRedraw() { return redrawRequested; }
    void Redrew() { redrawRequested = false; }
private:
    bool NeedsClose = false;
    bool redrawView = false;
    bool redrawRequested = false;
    JavaScriptEngine *jsEngine{};
    // --- Internal tab renderers ---------------------------------------------
    void RenderConsoleTab (int panelX, int panelY, int panelW, int panelH);
    void RenderNetworkTab (int panelX, int panelY, int panelW, int panelH);

    void RenderInspectorTreeNode(const Node *node, int treeW);

    void RenderInspectorTab(int panelX, int panelY, int panelW, int panelH);
    const Node* selectedNode = nullptr;


    // Builds style rows for the currently selected inspector node.
    void BuildStyleRows();

    // --- Platform & UI ------------------------------------------------------
    std::shared_ptr<IRenderBackend> renderBackend;
    WindowID renderWindow = 0;
    std::unique_ptr<Platform>  platform;
    std::unique_ptr<UIManager> ui;
    std::string jsBuffer = "";
    int windowWidth;
    int windowHeight;
    bool isOpen = false;
    bool shiftHeld = false;

    // --- Tab state ----------------------------------------------------------
    int activeTab = 0;   // 0=Console, 1=Network, 2=Inspector
    static constexpr int TAB_BAR_H  = 30;
    static constexpr int TOOL_BAR_H = 32;

    // --- Console state ------------------------------------------------------
    std::vector<DebugLogEntry> logEntries;
    std::vector<DebugListRow>       consoleRows;   // rebuilt when logEntries changes
    bool   consoleAutoScroll    = true;
    bool   showLogs             = true;
    bool   showWarns            = true;
    bool   showErrors           = true;
    std::string consoleFilter;
    bool   consoleRowsDirty     = true;

    void RebuildConsoleRows();

    // --- Network state ------------------------------------------------------
    std::vector<DebugNetEntry> netEntries;
    std::vector<DebugListRow>       networkRows;
    bool networkRowsDirty = true;

    void RebuildNetworkRows();

    // --- Inspector state ----------------------------------------------------
    const Node*           domRoot = nullptr;
    std::vector<DebugListRow>  inspectorRows;
    std::vector<const Node*> inspectorNodes;   // parallel to inspectorRows
    bool                  inspectorDirty = true;

    std::vector<DebugListRow>  styleRows;

    void RebuildInspectorRows();

    UI_Image minimize;

    UI_Image maximize;
    UI_Image Return; // return from maximize
};
