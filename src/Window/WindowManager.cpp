#include "WindowManager.h"
#include "Debug/DebugWindowManager.h"
#include "Debug/DebugOverlayRenderer.h"
#include "Platform/EventDispatcher.h"

#include <filesystem>
#include <functional>
#include <iostream>
#include <thread>

#include "Layout/Context/FontManager.h"
#include "Curl/CurlGrabber.h"

WindowManager::WindowManager(const int width, const int height)
    : renderBackend(IRenderBackend::GetRenderBackend(PreferredBackend::OpenGL)),
      debugWindow(std::make_unique<DebugWindowManager>(900, 500)),
      fallbackFontPrimary("Fonts/arial/ARIAL.TTF", 200),
    fallbackFontSymbol("Fonts/segoeui.ttf", 200),
fallbackFontEmoji("Fonts/seguiemj.ttf", 200), moduleCache(std::filesystem::current_path().string() + "/cache"),
fallback(fallbackFontPrimary, fallbackFontSymbol, fallbackFontEmoji), jsEngine(moduleCache)
{
    platform = CreatePlatform();

    if (!platform->OpenWindow(WIDTH, HEIGHT, "Browser", true)) {
        std::cerr << "Failed to open window" << std::endl;
        return;
    }

    renderWindow = renderBackend->RegisterWindow(platform.get());
    renderer     = std::make_unique<RendererSurface>(width != 0 ? width : 800, height != 0 ? height : 600);
    ui_manager   = std::make_unique<UIManager>(WIDTH, TOP_WIDTH);

    renderBackend->AttachRenderTarget(renderWindow, renderer->GetTargetID());

    platform->SetMinimumSize(500, 500);
    platform->SetTopBarHeight({0, 0, platform->GetWidth(), TOP_WIDTH});

    tabs.push_back({ "Example Domain", "http://localhost:5173/", "Example Domain" });

    // Cache local asset handles
    minimize = ui_manager->MakeImage("./Assets/Icons/minus.svg", 27, 27);
    maximize = ui_manager->MakeImage("./Assets/Icons/maximize.svg", 27, 27);
    Return   = ui_manager->MakeImage("./Assets/Icons/minimize.svg", 27, 27);
    plus     = ui_manager->MakeImage("./Assets/Icons/plus.svg", 27, 27);
    close    = ui_manager->MakeImage("./Assets/Icons/x.svg", 27, 27);
    forward  = ui_manager->MakeImage("./Assets/Icons/arrow-right.svg", 20, 20);
    back     = ui_manager->MakeImage("./Assets/Icons/arrow-left.svg", 20, 20);
    reload   = ui_manager->MakeImage("./Assets/Icons/rotate-cw.svg", 20, 20);

    FontManager::setFallbackFont(&fallback);
    FontManager::AddFont("Arial", FontGroup(
        std::make_shared<Font>("Fonts/arial/ARIAL.TTF",   16),
        std::make_shared<Font>("Fonts/arial/ARIALI.TTF",  16),
        std::make_shared<Font>("Fonts/arial/ARIALBD.TTF", 16),
        std::make_shared<Font>("Fonts/arial/ARIALBI.TTF", 16)
    ));

    int targetWidth  = renderer->GetWidth();
    int targetHeight = renderer->GetHeight() - TOP_WIDTH;

    for (auto& tab : tabs) {
        tab.manager = std::make_unique<ViewportManager>(targetWidth, targetHeight, jsEngine, fallback, platform.get());
        tab.manager->SetLink(tab.url);
        tab.manager->Init();
        tab.manager->Update();
    }
    debugWindow->SetScriptEntries(tabs[activeTabIndex].manager->GetScriptEntries());

    OnRender = [this]() {
        renderBackend->BeginFrame();

        int currentWidth  = platform->GetWidth();
        int currentHeight = platform->GetHeight();

        renderer->Resize(currentWidth, currentHeight);
        ui_manager->Resize(currentWidth, TOP_WIDTH);
        renderer->Clear(Color(255, 255, 255, 255));

        UpdateUI();

        auto& activeManager = tabs[activeTabIndex].manager;
        activeManager->Step();
        int targetContentHeight = std::max(0, currentHeight - TOP_WIDTH);

        activeManager->Resize(currentWidth, targetContentHeight);
        activeManager->OnRender(currentWidth, targetContentHeight);
        if (activeManager->DOMUpdated()) {
            FeedDebugDOM();
            debugWindow->SetScriptEntries(activeManager->GetScriptEntries());
        }
        // Blit UI layer
        renderer->BlitFrom(*ui_manager->GetRenderer(), 0, 0, 0, 0, renderer->GetWidth(), TOP_WIDTH);

        // Blit Web Content layer
        renderer->BlitFrom(activeManager->GetRenderer(), 0, TOP_WIDTH, 0, 0, activeManager->GetWidth(), activeManager->GetHeight());

        // Process Box-Model debugging overlay from separate module
        if (auto node = debugWindow->GetSelectedNode()) {
            DebugOverlayRenderer::DrawBoxModel(*renderer, node, TOP_WIDTH);
        }

        if (debugWindow->IsOpen()) {
            debugWindow->Render();
        }

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

void WindowManager::FeedDebugDOM() const {
    if (!debugWindow->IsOpen()) return;
    debugWindow->FeedDOM(tabs[activeTabIndex].manager->GetDOMRoot());
}

void WindowManager::Run() {
    for (auto& tab : tabs) {
        tab.manager->StartScripts();
    }

    while (platform->IsRunning()) {
        bool polledAnyEvent = EventDispatcher::DispatchEvents(*this);

        bool shouldRender = platform->needsRedraw || polledAnyEvent || debugWindow->Redraw();

        OnRender();
        platform->needsRedraw = false;
        if (shouldRender) {

        }

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

void WindowManager::UpdateUI() {
    ui_manager->BeginFrame();
    ui_manager->BeginWindow("Top_Window", {0, 0, (float)platform->GetWidth(), TOP_WIDTH});

    for (size_t i = 0; i < tabs.size(); ++i) {
        ui_manager->PushID("Tabs: " + std::to_string(i));
        bool isActive = (i == activeTabIndex);
        auto title = tabs[i].manager->GetTitle();

        auto cursorBefore = ui_manager->GetCursor();
        const int tabWidth = 140;
        const int tabHeight = 35;

        auto tabResult = ui_manager->Tab(tabs[i].id, title, isActive, tabWidth, tabHeight);
        ui_manager->SameLine();

        auto TabCursor = ui_manager->GetCursor();
        ui_manager->SetCursor(cursorBefore.x + (tabWidth - 4 - 27), cursorBefore.y + 4);

        bool CloseResult = false;
        if (ui_manager->IsMouseOver(cursorBefore.x + (tabWidth - 4 - 27), cursorBefore.y + 4, 27, 28) && ui_manager->IsMouseClicked()) {
            CloseResult = true;
        }
        ui_manager->Button("x", 27, 27);
        ui_manager->SetCursor(TabCursor.x, TabCursor.y);

        if (CloseResult) {
            tabs.erase(tabs.begin() + i);
            if (activeTabIndex >= tabs.size() && !tabs.empty()) {
                activeTabIndex = tabs.size() - 1;
            }
            ui_manager->PopID();
            break;
        }
        if (tabResult.activated && !isActive) {
            activeTabIndex = i;
            tabs[activeTabIndex].manager->Resize(platform->GetWidth(), platform->GetHeight() - TOP_WIDTH);
            FeedDebugDOM();
            CurlGrabber::ResetLog();
        }

        ui_manager->PopID();
        if (i + 1 < tabs.size()) ui_manager->SameLine();
    }

    if (ui_manager->SvgButton("plus", plus, 35, 35).activated) {
        TabState newTab{"New Tab", "https://example.com/", "New Tab", nullptr};

        newTab.manager = std::make_unique<ViewportManager>(renderer->GetWidth(), renderer->GetHeight() - TOP_WIDTH, jsEngine, fallback, platform.get());
        newTab.manager->SetLink(newTab.url);
        newTab.manager->Init();
        newTab.manager->Update();
        newTab.manager->StartScripts();
        CurlGrabber::ResetLog();
        tabs.push_back(std::move(newTab));
        activeTabIndex = tabs.size() - 1;
    }

    auto emptySpace = ui_manager->GetCursor();
    int windowWidth = platform->GetWidth();
    auto saved = ui_manager->GetCursor();

    ui_manager->SetCursor(windowWidth - 123, saved.y);

    if (ui_manager->SvgButton("minimize", minimize, 35, 35).activated) {
        platform->MinimizeWindow();
    }
    ui_manager->SameLine(0);

    if (ui_manager->SvgButton("minmax", platform->Is_WindowZoomed() ? Return : maximize, 35, 35).activated) {
        platform->MaximizeOrRestoreWindow();
    }
    ui_manager->SameLine(0);

    if (ui_manager->SvgButton("close", close, 35, 35).activated) {
        if (debugWindow->IsOpen()) debugWindow->Close();
        platform->CloseWindow();
    }

    ui_manager->SetRowHeight(38);
    platform->SetTopBarHeight({emptySpace.x, 0, platform->GetWidth() - emptySpace.x - 89, 38});

    // Row 2 Layout
    ui_manager->NewLine(0);
    ui_manager->RowBackground(TOP_WIDTH - 38, Color(220, 220, 220, 255));
    ui_manager->AdvanceCursorY(6);
    auto& activeManager = tabs[activeTabIndex].manager;

    if (ui_manager->SvgButton("backward", back, 28, 28).activated) {}
    ui_manager->SameLine();
    if (ui_manager->SvgButton("forward", forward, 28, 28).activated) {}
    ui_manager->SameLine();
    if (ui_manager->SvgButton("refresh", reload, 28, 28).activated) {
        CurlGrabber::ResetLog();
        activeManager->SetLink(tabs[activeTabIndex].url);
        activeManager->Update();
        activeManager->StartScripts();
        FeedDebugDOM();
        debugWindow->SetScriptEntries(activeManager->GetScriptEntries());
    }
    ui_manager->SameLine();

    int remainingWidth = std::max(200, renderer->GetWidth() - 130);
    std::string& activeUrl = tabs[activeTabIndex].url;
    if (ui_manager->AddressBar("URLInput", activeUrl, remainingWidth, 28).activated) {
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