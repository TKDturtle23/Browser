#include "ViewportManager.h"
#include "Curl/UrlUtils.h"
#include "Text/TextSelector.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <vector>

#include "Curl/BrowserCacheManager.h"
#include "Curl/CurlGrabber.h"
#include "../Parser.h"
#include "../Tokenizer.h"
#include "../CSS/CSSParser.h"
#include "ImageViewer.h"
#include "Images/SvgViewer.h"
#include "JavaScriptEngine/JavaScriptEngine.h"
#include "JavaScriptEngine/JS_Functions.h"
#include "Debug/Timer.h"
#include "Platform/Platform.h"

using namespace Engine::Utils;
using namespace Engine::UI;

// Clean standalone translation internal helpers
namespace {
    void FixParentPointers(Node*node) {
        for (auto& child : node->children) {
            child->parent = node;
            FixParentPointers(child.get());
        }
    }

    bool NeedReconstruct(Node* dom) {
        if (!dom) return false;
        if (dom->reconstruct) return true;
        for (const auto& c : dom->children) {
            if (NeedReconstruct(c.get())) return true;
        }
        return false;
    }

    void LoadImageResourceSync(Node& imgNode, const std::string& absoluteUrl, BrowserCacheManager& cache, int layoutWidth = 0, int layoutHeight = 0) {
        auto imgData = std::make_shared<ImageData>();
        imgNode.imageData = imgData;

        std::string webData = cache.GetResource(absoluteUrl);
        if (webData.empty()) return;

        std::string localFilePath = ConvertUrlToCachePath(absoluteUrl);
        std::ofstream outFile(localFilePath, std::ios::binary);
        if (outFile) {
            outFile.write(webData.data(), webData.size());
            outFile.close();
        }

        int width = 0, height = 0, channels;
        std::vector<uint8_t> rawBytes;
        bool isSvg = SvgViewer::IsSvg(std::vector<uint8_t>(webData.begin(), webData.end()));

        if (isSvg) {
            SvgViewer viewer;
            if (layoutWidth == 0 || layoutHeight == 0) {
                viewer.GetIntrinsicDimensions(std::vector<uint8_t>(webData.begin(), webData.end()), layoutWidth, layoutHeight);
            }
            width = layoutWidth;
            height = layoutHeight;
            rawBytes = viewer.GetPixels(std::vector<uint8_t>(webData.begin(), webData.end()), layoutWidth, layoutHeight, channels);
            viewer.clean();
        } else {
            rawBytes = load_image(localFilePath, width, height, nullptr);
        }

        if (rawBytes.empty() || width <= 0 || height <= 0) return;

        imgData->intrinsicWidth = width;
        imgData->intrinsicHeight = height;
        imgData->pixels.reserve(width * height);

        for (size_t i = 0; i < rawBytes.size(); i += 4) {
            if (i + 3 < rawBytes.size()) {
                Color pixel;
                if (isSvg) {
                    pixel.r = rawBytes[i];     pixel.g = rawBytes[i + 1];
                    pixel.b = rawBytes[i + 2]; pixel.a = rawBytes[i + 3];
                } else {
                    pixel.b = rawBytes[i];     pixel.g = rawBytes[i + 1];
                    pixel.r = rawBytes[i + 2]; pixel.a = rawBytes[i + 3];
                }
                imgData->pixels.push_back(pixel);
            }
        }
        imgData->isLoaded = true;
    }
}

ViewportManager::ViewportManager(const int width, const int height, JavaScriptEngine& engine, FallbackFonts& fallbackFont, Platform *platform)
    : renderer(width, height),
      layout(renderer),
      cache(std::filesystem::current_path().string() + "/cache"), 
      dom(), 
      engine(engine), 
      layoutRenderer(renderer, fallbackFont),
    plat(platform)
{

}

ViewportManager::~ViewportManager() {
    engine.destroy_tab_context(tabContext);
}

void ViewportManager::MoveMouse(int x, int y) {
    if (!IO.is_dragging && IO.Mouse_clicked) {
        IO.mouse_drag_x = x;
        IO.mouse_drag_y = y;
        IO.is_dragging = true;
    }
    IO.mouse_x = x;
    IO.mouse_y = y;
}

void ViewportManager::SetMouseClicked(bool clicked) {
    IO.Mouse_clicked = clicked;
    if (!IO.Mouse_clicked) {
        IO.is_dragging = false;
        IO.dragged = true;
    }
}

void ViewportManager::SetShiftHeld(bool held) { IO.shift_held = held; }
void ViewportManager::SetCtrlHeld(bool held)  { IO.ctrl_held = held; }

LayoutBox* ViewportManager::HitTest(int x, int y) {
    return layoutRenderer.HitTest(x, y);
}

std::vector<EventListener> ViewportManager::GetWindowListeners(const std::string &type) {
    return windowListeners[type];
}


void ViewportManager::FindTitle() {
    if (auto title_node = Parser::FindNodeByTag(&dom, "title")) {
        for (auto &c : title_node->children) {
            if (c->type == NodeType::Text) {
                title = c->text;
                return;
            }
        }
    }
    title = "";
}

void ViewportManager::Init() {
    if (CurrentLink.empty()) {
        std::cerr << "No link set!" << std::endl;
        exit(1);
    }


    CurlGrabber::Init();
    std::filesystem::create_directories(std::filesystem::current_path().string() + "/cache");

    std::string response = cache.GetResource(CurrentLink);
    auto tokens = tokenizer.tokenize(response);
    dom = parser.Parse(tokens);
    FixParentPointers(&dom);
    tabContext = engine.create_tab_context(CurrentLink, &dom);
    FindTitle();
    ApplyAndLayout();
}

void ViewportManager::SetLink(const std::string &Link) {
    CurrentLink = Link;
    LinkChanged = true;
}

void ViewportManager::Update() {
    if (LinkChanged) {
        LinkChanged = false;
        if (tabContext) {
            engine.destroy_tab_context(tabContext);
            tabContext = engine.create_tab_context(CurrentLink, &dom);
        }
        Init();
    }

    UpdateNeeded |= NeedReconstruct(&dom);

    if (UpdateNeeded) {
        Rendered = true;
        UpdateNeeded = false;

        CSSParser cssParser;
        std::vector<CSSRule> rules;

        CollectStyles(dom, cssParser, cache, CurrentLink, rules);

        // 2. Reset Styles
        std::function<void(Node&)> resetStyles = [&](Node& node) {
            node.specifiedStyle = Style{};
            for (auto& child : node.children) resetStyles(*child);
        };
        resetStyles(dom);

        // 3. Apply CSS & Compute Styles
        cssParser.Apply(rules, dom, renderer.GetWidth(), renderer.GetHeight());
        ComputeStyle(dom);

        FindTitle();
        FixParentPointers(&dom);
        ComputeStyle(dom);
        layout.Update(dom);
    }
}

void ViewportManager::Resize(int width, int height) {
    if (renderer.GetWidth() != width || renderer.GetHeight() != height) {
        renderer.Resize(width, height);
        UpdateNeeded = true;
    }
}

void ViewportManager::Step() {
    JavascriptFunctions::SetNewContext({&dom, &title, 0, windowListeners});
    engine.set_active_context(tabContext, CurrentLink);
    engine.Step();
}

void ViewportManager::Render() {
    auto& root = layout.GetRoot();


    TextSelector::UpdateAndApplySelection(root, IO, selection, layoutRenderer, plat);

    layoutRenderer.RenderRoot(root);
}
void ViewportManager::CollectStyles(
    Node& node,
    CSSParser& cssParser,
    BrowserCacheManager& cache,
    const std::string& currentLink,
    std::vector<CSSRule>& rules)
{
    auto it = node.attributes.find("rel");

    if (node.HasAttribute("style") ||
        (node.type == NodeType::Element &&
         (node.tag == "style" ||
          (it != node.attributes.end() &&
           it->second == "stylesheet"))))
    {
        if (node.HasAttribute("style")) {
            auto parsed = cssParser.Parse(
                node.GetAttribute("style"),
                true
            );

            rules.insert(
                rules.end(),
                parsed.begin(),
                parsed.end()
            );
        }

        if (it != node.attributes.end()) {
            auto ref = node.attributes.find("href");

            if (ref != node.attributes.end()) {
                std::string res = cache.GetResource(
                    ResolveUrl(currentLink, ref->second)
                );

                auto parsed = cssParser.Parse(res, false);

                rules.insert(
                    rules.end(),
                    parsed.begin(),
                    parsed.end()
                );
            }
        }

        for (auto& child : node.children) {
            if (child->type == NodeType::Text) {
                auto parsed = cssParser.Parse(
                    child->text,
                    false
                );

                rules.insert(
                    rules.end(),
                    parsed.begin(),
                    parsed.end()
                );
            }
        }
    }

    for (auto& child : node.children) {
        CollectStyles(
            *child,
            cssParser,
            cache,
            currentLink,
            rules
        );
    }
}
void ViewportManager::ApplyAndLayout() {
    scriptEntries.clear();
    JavascriptFunctions::SetNewContext({&dom, &title, 0, windowListeners});

    CSSParser cssParser;
    std::vector<CSSRule> rules;

    CollectStyles(dom, cssParser, cache, CurrentLink, rules);

    // 2. Reset Styles
    std::function<void(Node&)> resetStyles = [&](Node& node) {
        node.specifiedStyle = Style{};
        for (auto& child : node.children) resetStyles(*child);
    };
    resetStyles(dom);

    // 3. Apply CSS & Compute Styles
    cssParser.Apply(rules, dom, renderer.GetWidth(), renderer.GetHeight());
    ComputeStyle(dom);

    // 4. Discover and Sync Load Images
    std::function<void(Node&)> discoverAndLoadImages = [&](Node& node) {
        if (node.type == NodeType::Element && node.tag == "img") {
            auto srcIt = node.attributes.find("src");
            if (srcIt != node.attributes.end() && !node.imageData) {
                auto widthIt = node.attributes.find("width");
                auto heightIt = node.attributes.find("height");
                int w = (widthIt != node.attributes.end()) ? std::stoi(widthIt->second) : 0;
                int h = (heightIt != node.attributes.end()) ? std::stoi(heightIt->second) : 0;
                
                LoadImageResourceSync(node, ResolveUrl(CurrentLink, srcIt->second), cache, w, h);
            }
        }
        for (auto& child : node.children) discoverAndLoadImages(*child);
    };
    discoverAndLoadImages(dom);

    // 5. Discover Javascript
    std::function<void(Node&)> discoverAndRunJavascript = [&](Node& node) {
        if (node.type == NodeType::Element && node.tag == "script") {
            auto srcIt = node.attributes.find("src");
            if (srcIt != node.attributes.end()) {
                node.code = cache.GetResource(ResolveUrl(CurrentLink, srcIt->second));
                node.script_name = srcIt->second;
                scriptEntries.emplace_back(srcIt->second, node.code);
            } else {
                std::string inlineScript = "";
                for (auto& child : node.children) {
                    if (child->type == NodeType::Text) inlineScript += child->text;
                }
                node.code = inlineScript;
                node.script_name = "inline";
                scriptEntries.emplace_back("inline", node.code);
            }
        }
        for (auto& child : node.children) discoverAndRunJavascript(*child);
    };

    discoverAndRunJavascript(dom);

    // 6. Layout Updates
    layout.Update(dom);
    layoutRenderer.UpdateDom(&dom);

}

void ViewportManager::OnRender(int width, int height) {
    Step();
    Update();
    Render();
}

void ViewportManager::RunNodeScripts(Node &node) {
    if (!node.code.empty()) {
        engine.Run(node.code, node.script_name, node.attributes.find("type") != node.attributes.end() && node.attributes.at("type") == "module");
    }
    for (auto &child : node.children) {
        RunNodeScripts(*child);
    }
}

void ViewportManager::StartScripts() {
    JavascriptFunctions::SetNewContext({&dom, &title});
    engine.set_active_context(tabContext, CurrentLink);
    RunNodeScripts(dom);
}