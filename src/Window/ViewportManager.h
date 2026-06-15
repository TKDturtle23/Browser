//
// Created by tkdtu on 5/28/2026.
//

#ifndef BROWSER_VIEWPORTMANAGER_H
#define BROWSER_VIEWPORTMANAGER_H
#include "../Curl/BrowserCacheManager.h"
#include "../Layout/LayoutGenerator.h"
#include "../Render/Renderer.h"
#include "JavaScriptEngine/JavaScriptEngine.h"
#include "Layout/LayoutRenderer.h"

struct ViewportIO
{
    int mouse_drag_x = 0, mouse_drag_y = 0;
    int mouse_x = -1, mouse_y = -1;
    bool is_dragging = false;
    bool dragged = false;
    bool mouse_down_this_frame = false;
    bool Mouse_clicked = false;

    bool shift_held = false;
    bool ctrl_held = false;
};
struct TextPosition {
    LayoutBox* box = nullptr;
    int offset = 0;
    bool valid = false;
};

struct PersistentSelection {

    bool active = false;

    TextHitResult start;

    TextHitResult end;

    bool caretVisible = false;

    bool dragging = false;
};
class ViewportManager {
public:

    ViewportManager(int width, int height, JavaScriptEngine &engine, FallbackFonts &fallbackFont, Platform *platform);

    ~ViewportManager();

    void MoveMouse(int x, int y);
    void SetMouseClicked(bool clicked);
    void SetShiftHeld(bool held);
    void SetCtrlHeld(bool held);

    void Init();
    void SetLink(const std::string &Link);
    void Update();
    void Resize(int width, int height);

    void Step();

    void Render();

    void OnRender(int width, int height); // for resizing
    void RunNodeScripts(Node &node);

    int GetWidth() const {return renderer.GetWidth(); }
    int GetHeight() const {return renderer.GetHeight(); }
    RendererSurface& GetRenderer() { return renderer; }

    void StartScripts();
    const Node* GetDOMRoot() const {return &dom; }
    std::string GetTitle() {return title.empty() ? CurrentLink : title; }
    LayoutBox *HitTest(int x, int y);
private:
    Platform *plat;
    std::string title;
    void FindTitle();
    std::string CurrentLink;
    bool LinkChanged = false;
    bool UpdateNeeded = false;
    void ApplyAndLayout();

    JSContext* tabContext; // Each tab retains its unique context key handler pointer
    RendererSurface renderer;
    Tokenizer tokenizer;
    Parser parser;
    LayoutGenerator layout;
    BrowserCacheManager cache;
    JavaScriptEngine& engine;
    LayoutRenderer layoutRenderer;
    Node dom;

    PersistentSelection selection;
    ViewportIO IO;
};



#endif //BROWSER_VIEWPORTMANAGER_H
