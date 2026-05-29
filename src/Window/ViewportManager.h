//
// Created by tkdtu on 5/28/2026.
//

#ifndef BROWSER_VIEWPORTMANAGER_H
#define BROWSER_VIEWPORTMANAGER_H
#include "../BrowserCacheManager.h"
#include "../Layout/LayoutRenderer.h"
#include "../Render/Renderer.h"


class ViewportManager {
public:
    ViewportManager(int width, int height);
    void Init();
    void SetLink(const std::string &Link);
    void Update();
    void Resize(int width, int height);
    std::vector<Color> Render();

    std::vector<Color> OnRender(int width, int height); // for resizing
    int GetWidth() {return renderer.GetWidth(); }
    int GetHeight() {return renderer.GetHeight(); }
private:
    std::string CurrentLink;
    bool LinkChanged = false;
    bool UpdateNeeded = false;
    void ApplyAndLayout();


    Renderer renderer;
    Tokenizer tokenizer;
    Parser parser;
    LayoutRenderer layout;
    BrowserCacheManager cache;

    Node dom;
};



#endif //BROWSER_VIEWPORTMANAGER_H
