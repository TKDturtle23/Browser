#include "WindowManager.h"
#include "DebugWindowManager.h"

#include <filesystem>
#include <functional>
#include <iostream>
#include <thread>

#include "Layout/Context/FontManager.h"

#define TOP_WIDTH 88
#define WIDTH 800
#define HEIGHT 600
WindowManager::WindowManager(const int width, const int height)
    : renderBackend(IRenderBackend::GetRenderBackend(PreferredBackend::OpenGL)),
      debugWindow(std::make_unique<DebugWindowManager>(900, 500)),
fallbackFont("arial/ARIAL.TTF", 14)


{
    platform = CreatePlatform();

    if (!platform->OpenWindow(WIDTH, HEIGHT, "Browser", true)) {
        std::cerr << "Failed to open window" << std::endl;
        return;
    }

    renderWindow = renderBackend->RegisterWindow(platform.get());

    renderer = std::make_unique<RendererSurface>(width != 0 ? width : 800, height != 0 ? height : 600);
    ui_manager = std::make_unique<UIManager>(WIDTH, TOP_WIDTH);

    renderBackend->AttachRenderTarget(renderWindow, renderer->GetTargetID());

    platform->SetMinimumSize(500, 500);
    platform->SetTopBarHeight({0, 0, platform->GetWidth(), TOP_WIDTH});

    tabs.push_back({ "Example Domain", "http://localhost:8080/", "Example Domain" });

    minimize = ui_manager->MakeImage("./Assets/Icons/minus.svg", 27, 27);
    maximize = ui_manager->MakeImage("./Assets/Icons/maximize.svg", 27, 27);
    Return    = ui_manager->MakeImage("./Assets/Icons/minimize.svg",    27, 27);
    plus = ui_manager->MakeImage("./Assets/Icons/plus.svg", 27, 27);
    close = ui_manager->MakeImage("./Assets/Icons/x.svg", 27, 27);

    forward = ui_manager->MakeImage("./Assets/Icons/arrow-right.svg", 20, 20);
    back = ui_manager->MakeImage("./Assets/Icons/arrow-left.svg", 20, 20);
    reload = ui_manager->MakeImage("./Assets/Icons/rotate-cw.svg", 20, 20);

    FontManager::setFallbackFont(&fallbackFont);
    FontManager::AddFont("Arial", FontGroup(std::make_shared<Font>           ("arial/ARIAL.TTF",   16)
, std::make_shared<Font>      ("arial/ARIALI.TTF",  16)
, std::make_shared<Font>        ("arial/ARIALBD.TTF", 16)
, std::make_shared<Font>  ("arial/ARIALBI.TTF", 16)));
    int targetWidth  = renderer->GetWidth();
    int targetHeight = renderer->GetHeight() - TOP_WIDTH;

    for (auto& tab : tabs) {
        tab.manager = std::make_unique<ViewportManager>(targetWidth, targetHeight, jsEngine, fallbackFont);
        tab.manager->SetLink(tab.url);
        tab.manager->Init();
        tab.manager->Update();
    }

OnRender = [this]() {
        // 1. Initialize the backend canvas context for this frame pass
        renderBackend->BeginFrame();

        // 2. Snapshot current dimensions and scale presentation targets
        int currentWidth  = platform->GetWidth();
        int currentHeight = platform->GetHeight();

        renderer->Resize(currentWidth, currentHeight);
        ui_manager->Resize(currentWidth, TOP_WIDTH);

        // 3. Clear canvas with baseline background color
        renderer->Clear(Color(255, 255, 255, 255));

        // 4. Update UI component bounding zones
        UpdateUI();

        // 5. Update and scale the active tab Viewport Layout
        auto& activeManager = tabs[activeTabIndex].manager;
        int targetContentHeight = currentHeight - TOP_WIDTH;
        if (targetContentHeight < 0) targetContentHeight = 0;

        // Force viewport tree layout calculations to synchronize with new size
        activeManager->Resize(currentWidth, targetContentHeight);
        activeManager->OnRender(currentWidth, targetContentHeight);

        // 6. Blit the composited texture layers to the core surface
        renderer->BlitFrom(
            *ui_manager->GetRenderer(),
            0, 0,
            0, 0,
            renderer->GetWidth(),
            TOP_WIDTH
        );

        renderer->BlitFrom(
            activeManager->GetRenderer(),
            0, TOP_WIDTH,
            0, 0,
            activeManager->GetWidth(),
            activeManager->GetHeight()
        );

        // 7. Debugger Box Model Overlay
        if (auto node = debugWindow->GetSelectedNode()) {
            int constYOffset = TOP_WIDTH;

            int bx = node->renderData.box.x;
            int by = node->renderData.box.y + constYOffset;
            int bw = node->renderData.box.width;
            int bh = node->renderData.box.height;

            int pTop    = node->renderData.resolved_padding_top;
            int pBottom = node->renderData.resolved_padding_bottom;
            int pLeft   = node->renderData.resolved_padding_left;
            int pRight  = node->renderData.resolved_padding_right;

            int mTop    = node->renderData.resolved_margin_top;
            int mBottom = node->renderData.resolved_margin_bottom;
            int mLeft   = node->renderData.resolved_margin_left;
            int mRight  = node->renderData.resolved_margin_right;

            if (node->computedStyle.boxSizing == BoxSizing::BorderBox) {
                renderer->FillRect(bx - mLeft, by - mTop, bw + mLeft + mRight, bh + mTop + mBottom, Color(0, 255, 255, 64));
                renderer->FillRect(bx, by, bw, bh, Color(255, 0, 255, 64));
                int cx = bx + pLeft; int cy = by + pTop; int cw = bw - pLeft - pRight; int ch = bh - pTop - pBottom;
                renderer->FillRect(cx, cy, cw, ch, Color(255, 0, 0, 64));
            } else {
                int mx = bx - pLeft - mLeft; int my = by - pTop - mTop; int mw = bw + pLeft + pRight + mLeft + mRight; int mh = bh + pTop + pBottom + mTop + mBottom;
                renderer->FillRect(mx, my, mw, mh, Color(0, 255, 255, 64));
                int px = bx - pLeft; int py = by - pTop; int pw = bw + pLeft + pRight; int ph = bh + pTop + pBottom;
                renderer->FillRect(px, py, pw, ph, Color(255, 0, 255, 64));
                renderer->FillRect(bx, by, bw, bh, Color(255, 0, 0, 64));
            }
        }

        // 8. Draw secondary debug window windows over main surface if active
        if (debugWindow->IsOpen()) {
            debugWindow->Render();
        }

        // 9. Finalize frame pipeline commands and present to screen immediately
        renderBackend->EndFrame();
        renderBackend->Present();
    };
    platform->onRender = OnRender;

    debugWindow->FeedJS(&jsEngine);
}

WindowManager::~WindowManager() {
    if (renderBackend && renderWindow != 0) {
        renderBackend->UnregisterWindow(renderWindow);
    }
}

// ---------------------------------------------------------------------------
// Helper: rebuild the active tab's DOM feed into the debug window
// ---------------------------------------------------------------------------
void WindowManager::FeedDebugDOM() {
    if (!debugWindow->IsOpen()) return;
    const Node* root = tabs[activeTabIndex].manager->GetDOMRoot(); // implement on ViewportManager
    debugWindow->FeedDOM(root);
    
}

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
                renderer->Resize(event.width, event.height);
                ui_manager->Resize(event.width, TOP_WIDTH);
                tabs[activeTabIndex].manager->Resize(event.width, event.height - TOP_WIDTH);
                tabs[activeTabIndex].manager->Update();
                platform->needsRedraw = true;
            }
            else if (event.type == EventType::KeyPress) {
                if (event.key == Key::LShift || event.key == Key::RShift) {
                    ShiftPressed = true;
                }

                if (event.key == Key::F12) {
                    if (debugWindow->IsOpen()) {
                        debugWindow->Close();
                    } else {
                        debugWindow->Open();
                        FeedDebugDOM();
                    }
                }

                ui_manager->InjectKeyChar(event.key, ShiftPressed);
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
                        ui_manager->InjectMouseButton(true);
                    if (event.type == EventType::MouseButtonRelease && event.button == 1)
                        ui_manager->InjectMouseButton(false);
                    if (event.type == EventType::MouseMove) {
                        ui_manager->InjectMouseMove(event.x, event.y);
                        mouse_x = event.x;
                        mouse_y = event.y;
                    }
                } else {
                    if (event.type == EventType::MouseMove)
                    {
                        if (mouse_y > TOP_WIDTH)
                        {
                            tabs[activeTabIndex].manager->MoveMouse(event.x, event.y);
                        }
                        mouse_x = event.x;
                        mouse_y = event.y - TOP_WIDTH;
                    }
                    if (event.type == EventType::MouseButtonPress  && event.button == 1)
                        tabs[activeTabIndex].manager->SetMouseClicked(true);
                    if (event.type == EventType::MouseButtonRelease && event.button == 1)
                        tabs[activeTabIndex].manager->SetMouseClicked(false);
                }
                platform->needsRedraw = true;
            }
        }
        if (debugWindow->IsOpen()) {

            Event debugEvent;

            while (debugWindow->GetPlatform()->PollEvent(debugEvent)) {

                debugWindow->HandleEvent(debugEvent);
            }
        }

        bool shouldRender =
            platform->needsRedraw ||
            polledAnyEvent ||
            debugWindow->Redraw();
        if (debugWindow->IsOpen()) {
            shouldRender |= debugWindow->NeedsRedraw();
            debugWindow->Redrew();
        }

        if (shouldRender) {
            OnRender();


            platform->needsRedraw = false;
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(8)
        );

    }
}

void WindowManager::UpdateUI() {

    ui_manager->BeginFrame();
    ui_manager->BeginWindow("Top_Window", {0, 0, (float)platform->GetWidth(), TOP_WIDTH});



    for (size_t i = 0; i < tabs.size(); ++i) {
        ui_manager->PushID("Tabs: " + std::to_string(i));
        bool isActive = (i == activeTabIndex);
        auto title = tabs[i].manager->GetTitle();

        // 1. Snapshot the layout engine positions BEFORE doing anything
        auto cursorBefore = ui_manager->GetCursor();
        const int tabWidth = 140;
        const int tabHeight = 35;


        // 2. Intercept and run the Close Button handling code first!
        // This allows the button to steal hotID / activeID before the parent Tab component consumes it.

        // 3. Reset layout state back to original position to process the Tab item

        auto tabResult = ui_manager->Tab(tabs[i].id, title, isActive, tabWidth, tabHeight);
        ui_manager->SameLine();
        auto TabCursor = ui_manager->GetCursor();
        ui_manager->SetCursor(cursorBefore.x + (tabWidth - 4 - 27), cursorBefore.y + 4);
        bool CloseResult = false;
        if (ui_manager->IsMouseOver(cursorBefore.x + (tabWidth - 4 - 27), cursorBefore.y + 4, 27, 28) && ui_manager->IsMouseClicked()) { // hacking the event system
            CloseResult = true;
        }
        ui_manager->Button("x", 27, 27);

        ui_manager->SetCursor(TabCursor.x, TabCursor.y);

        // 4. Decoupled Business Logic Phase
        if (CloseResult) {
            // Handle tab destruction safely
            tabs.erase(tabs.begin() + i);

            // Correct the active index bounds safely
            if (activeTabIndex >= tabs.size() && !tabs.empty()) {
                activeTabIndex = tabs.size() - 1;
            }

            ui_manager->PopID();

            // Break out of this frame loop early to avoid indexing vector out of bounds mutations
            break;
        }
        else if (tabResult.activated && !isActive) {
            activeTabIndex = i;
            tabs[activeTabIndex].manager->Resize(platform->GetWidth(),
                                                 platform->GetHeight() - TOP_WIDTH);
            FeedDebugDOM();
            CurlGrabber::ResetLog();
        }

        ui_manager->PopID();

        // 5. Clean layout trailing line endings
        if (i + 1 < tabs.size()) {
            ui_manager->SameLine();
        }
    }

    // Draw the "+" button right next to the last tab
    if (ui_manager->SvgButton("plus", plus, 35, 35)) {
        int targetWidth  = renderer->GetWidth();
        int targetHeight = renderer->GetHeight() - TOP_WIDTH;

        TabState newTab{"New Tab", "https://example.com/", "New Tab", nullptr};
        newTab.manager = std::make_unique<ViewportManager>(targetWidth, targetHeight, jsEngine, fallbackFont);
        newTab.manager->SetLink(newTab.url);
        newTab.manager->Init();
        newTab.manager->Update();
        newTab.manager->StartScripts();
        CurlGrabber::ResetLog();
        tabs.push_back(std::move(newTab));
        activeTabIndex = tabs.size() - 1;
    }
    auto emptySpace = ui_manager->GetCursor();
    // --- Custom Window Controls (Top Right Corner) ---
    int windowWidth = platform->GetWidth();

    // Save current layout cursor position
    auto saved = ui_manager->GetCursor();

    // --- FIXED: Calculate drag zone space AFTER the "+" button, but BEFORE the next row ---
    // Position the window control cluster tightly against the right side
    // 3 buttons * 35px width = 105px total width + 5px right padding
    ui_manager->SetCursor(windowWidth - 123 /* padding */, saved.y);

    if (ui_manager->SvgButton("minimize", minimize,35, 35)) {
        platform->MinimizeWindow();
    }
    ui_manager->SameLine(0); // Pack them right next to each other

    if (ui_manager->SvgButton("minmax", platform->Is_WindowZoomed() ? Return : maximize, 35, 35)) {
        platform->MaximizeOrRestoreWindow();
    }
    ui_manager->SameLine(0);

    if (ui_manager->SvgButton("close", close, 35, 35)) {
        if (debugWindow->IsOpen()) {
            debugWindow->Close();
        }
        platform->CloseWindow();
    }

    // Restore the layout cursor position so Row 2 renders perfectly
   // ui_manager->SetCursor(saved.x, saved.y);
    ui_manager->SetRowHeight(38);
    platform->SetTopBarHeight({emptySpace.x, 0, platform->GetWidth() - emptySpace.x - 89, 38});

    // Note: Height set to 28 so it only captures Row 1's empty area!

    // --- ROW 2: Navigation & Address Bar ---
    ui_manager->NewLine(0);
    ui_manager->RowBackground(TOP_WIDTH - 38, Color(220, 220, 220, 255));
    ui_manager->AdvanceCursorY(6); // padding
    auto& activeManager = tabs[activeTabIndex].manager;

    if (ui_manager->SvgButton("backward",  back, 28, 28)) { /* GoBack */ }
    ui_manager->SameLine();
    if (ui_manager->SvgButton("forward", forward, 28, 28)) { /* GoForward */ }
    ui_manager->SameLine();
    if (ui_manager->SvgButton("refresh", reload, 28, 28)) {
        CurlGrabber::ResetLog();
        activeManager->SetLink(tabs[activeTabIndex].url);
        activeManager->Update();
        activeManager->StartScripts();
        FeedDebugDOM();
    }
    ui_manager->SameLine();

    int remainingWidth = renderer->GetWidth() - 130;
    if (remainingWidth < 200) remainingWidth = 200;

    std::string& activeUrl = tabs[activeTabIndex].url;
    if (ui_manager->AddressBar("URLInput", activeUrl, remainingWidth, 28)) {
        CurlGrabber::ResetLog();
        activeManager->SetLink(activeUrl);
        activeManager->Update();
        FeedDebugDOM();
    }
    ui_manager->EndWindow();
    ui_manager->EndFrame();
}


void WindowManager::SetDebugNetworkEntries(const std::vector<DebugNetEntry>& entries) {
    debugWindow->SetNetworkEntries(entries);
}
