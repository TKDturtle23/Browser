//
// Created by tkdtu on 5/28/2026.
//

#ifndef BROWSER_WINDOWMANAGER_H
#define BROWSER_WINDOWMANAGER_H

#include <memory>
#include <vector>
#include <string>
#include <functional>

#include "Debug/DebugWindowManager.h"
#include "ViewportManager.h"
#include "../Platform/Platform.h"
#include "../Render/Renderer.h"
#include "UI/InterfaceManager.h"

// Constants consolidated to a single source of truth
#define WIDTH 800
#define HEIGHT 600
#define TOP_WIDTH 88 // Updated to match the 88 height used in layout loops

struct TabState {
    std::string id;
    std::string url;
    std::string title;
    std::unique_ptr<ViewportManager> manager;
};

class WindowManager {
    // Grant clean module permissions without cluttering with getters/setters
    friend class EventDispatcher;
    friend class DebugOverlayRenderer;

public:
    WindowManager(int width, int height);
    ~WindowManager();

    void Run();
    void FeedDebugDOM() const;
    void SetDebugNetworkEntries(const std::vector<DebugNetEntry>& entries);

    // Keep the core OS engine platform handle public for external lifecycle mapping
    std::unique_ptr<Platform> platform;

private:
    void UpdateUI();
    BrowserCacheManager moduleCache;
    // Structural Graphics Engine Dependencies
    std::shared_ptr<IRenderBackend> renderBackend;
    WindowID renderWindow = 0;
    std::unique_ptr<RendererSurface> renderer;
    std::unique_ptr<UIManager> ui_manager;
    std::function<void()> OnRender;

    // Tab & Script Orchestration State
    std::vector<TabState> tabs;
    size_t activeTabIndex = 0;
    JavaScriptEngine jsEngine;
    std::unique_ptr<DebugWindowManager> debugWindow;

    // UI Asset Handles
    UI_Image minimize;
    UI_Image maximize;
    UI_Image Return; // Return from maximized state
    UI_Image close;
    UI_Image reload;
    UI_Image forward;
    UI_Image back;
    UI_Image plus;

    // Global Fallback Typography Context
    Font fallbackFontPrimary;
    Font fallbackFontSymbol;
    Font fallbackFontEmoji;
    FallbackFonts fallback;

    // Input State Cache
    bool ShiftPressed = false;
    int mouse_x = 0;
    int mouse_y = 0;
};

#endif //BROWSER_WINDOWMANAGER_H