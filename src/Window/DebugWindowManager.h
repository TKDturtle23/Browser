#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "UI/InterfaceManager.h"
#include "Logger.h"
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

    // Replace the full network table (simplest; rebuild from your resource loader).
    void SetNetworkEntries(const std::vector<DebugNetEntry>& entries);

    // --- Per-frame call (call inside WindowManager::Run after main render) --
    // Pumps the debug window's own event loop and renders one frame.
    // Returns false if the window was closed by the user (so caller can set isOpen = false).
    bool Render();

private:
    JavaScriptEngine *jsEngine;
    // --- Internal tab renderers ---------------------------------------------
    void RenderConsoleTab (int panelX, int panelY, int panelW, int panelH);
    void RenderNetworkTab (int panelX, int panelY, int panelW, int panelH);
    void RenderInspectorTab(int panelX, int panelY, int panelW, int panelH);

    // Recursively flattens the DOM tree into inspectorRows / inspectorNodes.
    void FlattenDOM(const Node *node, int depth, bool Root);

    // Builds a ListRow for a single DOM node at a given indent depth.
    ListRow MakeNodeRow(const Node *node, int depth, bool &ChildrenHandled) const;

    // Builds style rows for the currently selected inspector node.
    void BuildStyleRows();

    // --- Platform & UI ------------------------------------------------------
    std::unique_ptr<Platform>             platform;
    std::unique_ptr<DebugInterfaceManager> ui;
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
    std::vector<ListRow>       consoleRows;   // rebuilt when logEntries changes
    bool   consoleAutoScroll    = true;
    bool   showLogs             = true;
    bool   showWarns            = true;
    bool   showErrors           = true;
    std::string consoleFilter;
    bool   consoleRowsDirty     = true;

    void RebuildConsoleRows();

    // --- Network state ------------------------------------------------------
    std::vector<DebugNetEntry> netEntries;
    std::vector<ListRow>       networkRows;
    bool networkRowsDirty = true;

    void RebuildNetworkRows();

    // --- Inspector state ----------------------------------------------------
    const Node*           domRoot = nullptr;
    std::vector<ListRow>  inspectorRows;
    std::vector<const Node*> inspectorNodes;   // parallel to inspectorRows
    bool                  inspectorDirty = true;

    std::vector<ListRow>  styleRows;
    const Node*           selectedNode = nullptr;

    void RebuildInspectorRows();
};