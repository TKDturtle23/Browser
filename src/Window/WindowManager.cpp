#include "WindowManager.h"
#include <filesystem>
#include <functional>
#include <iostream>
#include <thread>

#define TOP_WIDTH 80

WindowManager::WindowManager(const int width, const int height)
    : renderer(width != 0 ? width : 800, height != 0 ? height : 600),
      manager(renderer.GetWidth(), renderer.GetHeight() - TOP_WIDTH),
      ui_manager(renderer.GetWidth(), TOP_WIDTH) {

    platform = CreatePlatform();

    // FIX: Use constructor parameters instead of hardcoded 800x600
    if (!platform->OpenWindow(renderer.GetWidth(), renderer.GetHeight(), "Browser")) {
        std::cerr << "Failed to open window" << std::endl;
        return;
    }
    platform->SetMinimumSize(500, 500);
    tabs.push_back({ "localhost:8080", "http://localhost:8080/", "localhost:8080"});
    tabs.push_back({"Example Domain", "https://example.com/", "Example Domain"});

    manager.SetLink("http://localhost:8080/");
    manager.Init();
    manager.Update();

    OnRender = [this]() {
        renderer.Resize(platform->GetWidth(), platform->GetHeight());
        ui_manager.Resize(platform->GetWidth(), TOP_WIDTH);
        manager.Resize(platform->GetWidth(), platform->GetHeight() - TOP_WIDTH);

        // Render UI state layer
        UpdateUI();
        auto UI_pixels = ui_manager.GetFrontBuffer();
        renderer.CopyFromBuffer(0, 0, renderer.GetWidth(), TOP_WIDTH, UI_pixels);

        // Render web document content layout canvas layer
        const auto pixels = manager.OnRender(platform->GetWidth(), platform->GetHeight() - TOP_WIDTH);
        renderer.CopyFromBuffer(0, TOP_WIDTH, manager.GetWidth(), manager.GetHeight(), pixels);

        renderer.Present();
        platform->Present(renderer.GetFrontBuffer());
    };
    platform->onRender = OnRender;
}

WindowManager::~WindowManager() {}

void WindowManager::Run() {
    while (platform->IsRunning()) {
        Event event;
        bool polledAnyEvent = false;

        while (platform->PollEvent(event)) {
            polledAnyEvent = true;

            // Route Mouse Events based on Y-coordinate geometry split
            bool isMouseEvent = (event.type == EventType::MouseButtonPress ||
                                 event.type == EventType::MouseButtonRelease ||
                                 event.type == EventType::MouseMove);

            bool hitUITopBar = isMouseEvent && (event.y < TOP_WIDTH);

            if (event.type == EventType::Resize) {
                renderer.Resize(event.width, event.height);
                manager.Resize(event.width, event.height - TOP_WIDTH);
                ui_manager.Resize(event.width, TOP_WIDTH);
                manager.Update();
                platform->needsRedraw = true;
            }
            else if (event.type == EventType::KeyPress) {
                if (event.key == Key::LShift || event.key == Key::RShift) {
                    ShiftPressed = true;
                }
                // Focus rule: Send typing to UI if address bar is active, else to web page
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
                    if (event.type == EventType::MouseButtonPress && event.button == 1) ui_manager.InjectMouseButton(true);
                    if (event.type == EventType::MouseButtonRelease && event.button == 1) ui_manager.InjectMouseButton(false);
                    if (event.type == EventType::MouseMove) ui_manager.InjectMouseMove(event.x, event.y);
                } else {
                    // FIX: Forward web-content clicks to the page manager, adjusting for the UI offset
                    // manager.InjectMouse(event.x, event.y - TOP_WIDTH, ...);
                }
                platform->needsRedraw = true;
            }
        }

        // FIX: Performance Saver. Only render if something actually changed.
        if (platform->needsRedraw || polledAnyEvent) {
            OnRender();
            platform->needsRedraw = false;
        }

        // Relax CPU when idle (increase sleep time slightly if no events are happening)
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

void WindowManager::UpdateUI() {
    ui_manager.BeginFrame();

    // --- ROW 1: Tabs Bar ---
    for (size_t i = 0; i < tabs.size(); ++i) {
        bool isActive = (i == activeTabIndex);

        if (ui_manager.Tab(tabs[i].id, tabs[i].title, isActive, 140, 28)) {
            if (!isActive) {
                // FIX: Only update active tab index and swap document context.
                // Don't force a fresh re-download of the URL unless intended.
                activeTabIndex = i;
                manager.SetLink(tabs[i].url);
                manager.Update();
            }
        }
        ui_manager.SameLine();
    }
    if (ui_manager.Button("+", 28, 28)) {
        tabs.push_back({"New Tab", "https://google.com/", "New Tab"});
    }

    // --- ROW 2: Navigation & Address Bar ---
    ui_manager.NewLine(6);

    if (ui_manager.Button("<-", 30, 28)) {
        std::cout << "Back Navigation Pressed" << std::endl;
        // manager.GoBack();
    }
    ui_manager.SameLine();
    if (ui_manager.Button("->", 30, 28)) {
        std::cout << "Forward Navigation Pressed" << std::endl;
        // manager.GoForward();
    }
    ui_manager.SameLine();
    if (ui_manager.Button("R", 30, 28)) { // Added Refresh Button
        manager.SetLink(tabs[activeTabIndex].url);
        manager.Update();
    }
    ui_manager.SameLine();

    // FIX: Correct layout math. Row 2 width depends on buttons, not tabs!
    // Buttons total width: (30 * 3) + padding gaps = ~110px
    int remainingWidth = renderer.GetWidth() - 130;
    if (remainingWidth < 200) remainingWidth = 200;

    std::string& activeUrl = tabs[activeTabIndex].url;

    if (ui_manager.AddressBar("URLInput", activeUrl, remainingWidth, 28)) {
        std::cout << "Navigating Active Tab [" << activeTabIndex << "] to: " << activeUrl << std::endl;
        manager.SetLink(activeUrl);
        manager.Update();
    }

    ui_manager.EndFrame();
}