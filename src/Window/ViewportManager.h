//
// Created by tkdtu on 5/28/2026.
//

#ifndef BROWSER_VIEWPORTMANAGER_H
#define BROWSER_VIEWPORTMANAGER_H
#include "../BrowserCacheManager.h"
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

    bool Mouse_clicked = false;
};
class ViewportManager {
public:

    ViewportManager(int width, int height, JavaScriptEngine &engine, Font &fallbackFont);

    ~ViewportManager();

    void MoveMouse(int x, int y);
    void SetMouseClicked(bool clicked);


    void Init();
    void SetLink(const std::string &Link);
    void Update();
    void Resize(int width, int height);

    void Step();

    void Render();

    void OnRender(int width, int height); // for resizing
    void RunNodeScripts(Node &node);

    int GetWidth() {return renderer.GetWidth(); }
    int GetHeight() {return renderer.GetHeight(); }
    RendererSurface& GetRenderer() { return renderer; }

    void StartScripts();
    const Node* GetDOMRoot() {return &dom; }
    std::string GetTitle() {return title.empty() ? CurrentLink : title; }
    LayoutBox *HitTest(int x, int y);
private:
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

    ViewportIO IO;
};



#endif //BROWSER_VIEWPORTMANAGER_H
