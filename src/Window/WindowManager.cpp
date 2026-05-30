#include "WindowManager.h"
#include "DebugWindowManager.h"

#include <filesystem>
#include <functional>
#include <iostream>
#include <thread>

#define TOP_WIDTH 80

WindowManager::WindowManager(const int width, const int height)
    : renderer(width != 0 ? width : 800, height != 0 ? height : 600),
      ui_manager(renderer.GetWidth(), TOP_WIDTH),
      debugWindow(std::make_unique<DebugWindowManager>(900, 500))
{
    platform = CreatePlatform();

    if (!platform->OpenWindow(renderer.GetWidth(), renderer.GetHeight(), "Browser")) {
        std::cerr << "Failed to open window" << std::endl;
        return;
    }
    platform->SetMinimumSize(500, 500);

    tabs.push_back({ "localhost:8080", "http://localhost:8080/", "localhost:8080" });
    tabs.push_back({ "Example Domain", "https://example.com/", "Example Domain" });

    int targetWidth  = renderer.GetWidth();
    int targetHeight = renderer.GetHeight() - TOP_WIDTH;

    for (auto& tab : tabs) {
        tab.manager = std::make_unique<ViewportManager>(targetWidth, targetHeight, jsEngine);
        tab.manager->SetLink(tab.url);
        tab.manager->Init();
        tab.manager->Update();
    }

    OnRender = [this]() {
        renderer.Resize(platform->GetWidth(), platform->GetHeight());
        ui_manager.Resize(platform->GetWidth(), TOP_WIDTH);

        UpdateUI();

        auto& activeManager = tabs[activeTabIndex].manager;

        activeManager->Resize(platform->GetWidth(), platform->GetHeight() - TOP_WIDTH);

        auto UI_pixels = ui_manager.GetFrontBuffer();
        renderer.CopyFromBuffer(0, 0, renderer.GetWidth(), TOP_WIDTH, UI_pixels);

        const auto pixels = activeManager->OnRender(platform->GetWidth(),
                                                     platform->GetHeight() - TOP_WIDTH);
        renderer.CopyFromBuffer(0, TOP_WIDTH,
                                activeManager->GetWidth(), activeManager->GetHeight(), pixels);

        renderer.Present();
        platform->Present(renderer.GetFrontBuffer());
    };
    platform->onRender = OnRender;

    debugWindow->FeedJS(&jsEngine);
}

WindowManager::~WindowManager() {}

// ---------------------------------------------------------------------------
// Helper: rebuild the active tab's DOM feed into the debug window
// ---------------------------------------------------------------------------
void WindowManager::FeedDebugDOM() {
    if (!debugWindow->IsOpen()) return;
    const Node* root = tabs[activeTabIndex].manager->GetDOMRoot(); // implement on ViewportManager
    debugWindow->FeedDOM(root);
    
}

// ---------------------------------------------------------------------------
// Run loop
// ---------------------------------------------------------------------------
void WindowManager::Run() {
    for (auto& tab : tabs) {
        tab.manager->StartScripts();
    }

    while (platform->IsRunning()) {
        Event event;
        bool polledAnyEvent = false;

        while (platform->PollEvent(event)) {
            polledAnyEvent = true;

            bool isMouseEvent = (event.type == EventType::MouseButtonPress  ||
                                 event.type == EventType::MouseButtonRelease ||
                                 event.type == EventType::MouseMove);

            bool hitUITopBar = isMouseEvent && (event.y < TOP_WIDTH);

            if (event.type == EventType::Resize) {
                renderer.Resize(event.width, event.height);
                ui_manager.Resize(event.width, TOP_WIDTH);
                tabs[activeTabIndex].manager->Resize(event.width, event.height - TOP_WIDTH);
                tabs[activeTabIndex].manager->Update();
                platform->needsRedraw = true;
            }
            else if (event.type == EventType::KeyPress) {
                if (event.key == Key::LShift || event.key == Key::RShift) {
                    ShiftPressed = true;
                }

                // --- F12: toggle debug window ---
                if (event.key == Key::F12) {
                    if (debugWindow->IsOpen()) {
                        debugWindow->Close();
                    } else {
                        debugWindow->Open();
                        FeedDebugDOM();  // Immediately populate the inspector
                    }
                }

                ui_manager.InjectKeyChar(event.key, ShiftPressed);
                platform->needsRedraw = true;
            }
            else if (event.type == EventType::KeyRelease) {
                if (event.key == Key::LShift || event.key == Key::RShift) {
                    ShiftPressed = false;
                }
            }
            else if (isMouseEvent) {
                if (hitUITopBar) {
                    if (event.type == EventType::MouseButtonPress  && event.button == 1)
                        ui_manager.InjectMouseButton(true);
                    if (event.type == EventType::MouseButtonRelease && event.button == 1)
                        ui_manager.InjectMouseButton(false);
                    if (event.type == EventType::MouseMove)
                        ui_manager.InjectMouseMove(event.x, event.y);
                } else {
                    // tabs[activeTabIndex].manager->InjectMouse(event.x, event.y - TOP_WIDTH, ...);
                }
                platform->needsRedraw = true;
            }
        }

        if (platform->needsRedraw || polledAnyEvent) {
            OnRender();
            platform->needsRedraw = false;
        }

        // --- Debug window render tick ---
        // FeedDOM every frame so the inspector stays live when the DOM changes.
        if (debugWindow->IsOpen()) {
            FeedDebugDOM();
            debugWindow->Render();   // returns false if user closed the window
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

// ---------------------------------------------------------------------------
// UpdateUI  (unchanged except debug-open indicator on the tab bar)
// ---------------------------------------------------------------------------
void WindowManager::UpdateUI() {
    ui_manager.BeginFrame();

    // --- ROW 1: Tabs Bar ---
    for (size_t i = 0; i < tabs.size(); ++i) {
        bool isActive = (i == activeTabIndex);
        auto title = tabs[i].manager->GetTitle();
        if (ui_manager.Tab(tabs[i].id, title, isActive, 140, 28)) {
            if (!isActive) {
                activeTabIndex = i;
                tabs[activeTabIndex].manager->Resize(platform->GetWidth(),
                                                     platform->GetHeight() - TOP_WIDTH);
                // Feed new tab's DOM to the debug window
                FeedDebugDOM();
                CurlGrabber::ResetLog();
            }
        }
        ui_manager.SameLine();
    }

    if (ui_manager.Button("+", 28, 28)) {
        int targetWidth  = renderer.GetWidth();
        int targetHeight = renderer.GetHeight() - TOP_WIDTH;

        TabState newTab{"New Tab", "https://example.com/", "New Tab", nullptr};
        newTab.manager = std::make_unique<ViewportManager>(targetWidth, targetHeight, jsEngine);
        newTab.manager->SetLink(newTab.url);
        newTab.manager->Init();
        newTab.manager->Update();
        newTab.manager->StartScripts();
        CurlGrabber::ResetLog();
        tabs.push_back(std::move(newTab));
        activeTabIndex = tabs.size() - 1;
    }

    // --- ROW 2: Navigation & Address Bar ---
    ui_manager.NewLine(6);

    auto& activeManager = tabs[activeTabIndex].manager;

    if (ui_manager.Button("<-", 30, 28)) {
        // activeManager->GoBack();
    }
    ui_manager.SameLine();
    if (ui_manager.Button("->", 30, 28)) {
        // activeManager->GoForward();
    }
    ui_manager.SameLine();

    if (ui_manager.Button("R", 30, 28)) {
        CurlGrabber::ResetLog();
        activeManager->SetLink(tabs[activeTabIndex].url);
        activeManager->Update();
        activeManager->StartScripts();
        FeedDebugDOM();   // DOM changed after reload

    }
    ui_manager.SameLine();

    int remainingWidth = renderer.GetWidth() - 130;
    if (remainingWidth < 200) remainingWidth = 200;

    std::string& activeUrl = tabs[activeTabIndex].url;

    if (ui_manager.AddressBar("URLInput", activeUrl, remainingWidth, 28)) {

        CurlGrabber::ResetLog();
        activeManager->SetLink(activeUrl);
        activeManager->Update();
        FeedDebugDOM();   // New page, new DOM
    }

    ui_manager.EndFrame();
}



void WindowManager::SetDebugNetworkEntries(const std::vector<DebugNetEntry>& entries) {
    debugWindow->SetNetworkEntries(entries);
}