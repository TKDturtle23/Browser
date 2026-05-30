//
// Created by tkdtu on 5/28/2026.
//

#ifndef BROWSER_WINDOWMANAGER_H
#define BROWSER_WINDOWMANAGER_H
#include <memory>

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

    void Run();

private:
    bool ShiftPressed = false;
    void UpdateUI();
    Renderer renderer;
    std::unique_ptr<Platform> platform;

    std::function<void()> OnRender;
    InterfaceManager ui_manager;

    std::string currentTabUrl = "localhost:8080";
    bool isTabActive = true;

    std::vector<TabState> tabs = {};
    size_t activeTabIndex = 0; // Tracks which tab is selected
    JavaScriptEngine jsEngine;
};


#endif //BROWSER_WINDOWMANAGER_H
