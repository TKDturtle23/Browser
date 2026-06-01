//
// Created by tkdtu on 5/28/2026.
//

#ifndef BROWSER_WINDOWMANAGER_H
#define BROWSER_WINDOWMANAGER_H
#include <memory>

#include "DebugWindowManager.h"
#include "ViewportManager.h"
#include "../Platform/Platform.h"
#include "../Render/Renderer.h"
#include "UI/InterfaceManager.h"

struct TabState {
    std::string id;

    std::string url;
    std::string title;
    std::unique_ptr<ViewportManager> manager;
};
class WindowManager {
public:
    WindowManager(int width, int height);
    ~WindowManager();

    void FeedDebugDOM();

    void Run();

private:
    bool ShiftPressed = false;
    void UpdateUI();


    void SetDebugNetworkEntries(const std::vector<DebugNetEntry> &entries);

    Renderer renderer;
    std::unique_ptr<Platform> platform;

    std::function<void()> OnRender;
    UIManager ui_manager;

    std::string currentTabUrl = "localhost:8080";
    bool isTabActive = true;

    std::vector<TabState> tabs = {};
    size_t activeTabIndex = 0; // Tracks which tab is selected
    JavaScriptEngine jsEngine;
    std::unique_ptr<DebugWindowManager> debugWindow;


    UI_Image minimize;

    UI_Image maximize;
    UI_Image Return; // return from maximize

    UI_Image close;
    UI_Image reload;
    UI_Image forward;
    UI_Image back;

    UI_Image plus;

    Font fallbackFont;
};


#endif //BROWSER_WINDOWMANAGER_H
