//
// Created by tkdtu on 5/28/2026.
//

#include "ViewportManager.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <ostream>

#include <string>
#include <thread>
#include "../BrowserCacheManager.h"
#include "../CurlGrabber.h"
#include "../Parser.h"
#include "../Tokenizer.h"
#include "../CSS/CSSParser.h"
#include "../Layout/LayoutGenerator.h"

#include "ImageViewer.h"
#include "Debug/Logger.h"
#include "Debug/Timer.h"
#include "Images/SvgViewer.h"
#include "JavaScriptEngine/JavaScriptEngine.h"
#include "JavaScriptEngine/JS_Functions.h"
#include "Layout/Context/FontManager.h"

static std::string ConvertUrlToCachePath(const std::string& url) {
    std::string safeName = url;
    // Replace characters that are illegal or problematic in file systems
    for (char& c : safeName) {
        if (c == '/' || c == '\\' || c == ':' || c == '?' || c == '*' || c == '&') {
            c = '_';
        }
    }

    // Ensure a local cache directory exists
    std::filesystem::create_directories("image_cache");
    return "image_cache/" + safeName;
}
static std::string ResolveUrl(const std::string& baseUrl, const std::string& relUrl) {
    // Case 1: The link is already absolute (e.g., starts with https:// or http://)
    if (relUrl.rfind("https://", 0) == 0 || relUrl.rfind("http://", 0) == 0) {
        return relUrl;
    }

    // Case 2: Protocol-relative links (e.g., "//github.githubassets.com/style.css")
    if (relUrl.rfind("//", 0) == 0) {
        // Extract protocol from baseUrl (default to https if missing)
        size_t protoEnd = baseUrl.find("://");
        std::string protocol = (protoEnd != std::string::npos) ? baseUrl.substr(0, protoEnd) : "https";
        return protocol + ":" + relUrl;
    }

    // Parse base URL components for relative handling
    size_t protoEnd = baseUrl.find("://");
    std::string protocol = "https";
    std::string rest = baseUrl;
    if (protoEnd != std::string::npos) {
        protocol = baseUrl.substr(0, protoEnd);
        rest = baseUrl.substr(protoEnd + 3);
    }

    size_t slashPos = rest.find('/');
    std::string host = (slashPos != std::string::npos) ? rest.substr(0, slashPos) : rest;
    std::string path = (slashPos != std::string::npos) ? rest.substr(slashPos) : "/";

    // Case 3: Root-relative links (e.g., "/assets/style.css")
    if (!relUrl.empty() && relUrl[0] == '/') {
        return protocol + "://" + host + relUrl;
    }

    // Case 4: Purely relative links (e.g., "style.css" or "../style.css")
    // Strip the current filename off the path to get the active directory
    size_t lastSlash = path.find_last_of('/');
    std::string dir = (lastSlash != std::string::npos) ? path.substr(0, lastSlash + 1) : "/";

    return protocol + "://" + host + dir + relUrl;
}
ViewportManager::ViewportManager(const int width, const int height, JavaScriptEngine& engine, Font& fallbackFont) : renderer(width, height),
                                                                      layout(renderer),
                                                                      cache(
                                                                          std::filesystem::current_path().string() +
                                                                          "/cache"), dom(), engine(engine), layoutRenderer(renderer) {
    tabContext = engine.create_tab_context();

}

ViewportManager::~ViewportManager() {
    engine.destroy_tab_context(tabContext);
}
void ViewportManager::FindTitle() {
    if (auto title_node = Parser::FindNodeByTag(&dom, "title")) { // get title
        for (auto &c : title_node->children) {
            if (c->type == NodeType::Text) {
                title = c->text;
            }
        }
    }
    else {
        title = "";
    }
}
void ViewportManager::Init() {

    if (CurrentLink.empty()) {
        std::cerr << "No link set!" << std::endl;
        exit(1);
    }



    CurlGrabber::Init();
    std::filesystem::create_directories(std::filesystem::current_path().string() + "/cache");

    std::string response = cache.GetResource(CurrentLink);

    Timer Token_Timer;
    Token_Timer.StartTimer();
    auto tokens = tokenizer.tokenize(response);
    Token_Timer.StopTimer();
    Logger::Log_Verbose("Tokenizing took %f milliseconds", "ViewportManager", 0, Token_Timer.GetElapsedMilliseconds());
    // parse once
    Timer Parser_Timer;
    Parser_Timer.StartTimer();
    dom = parser.Parse(tokens); // no renderer dependency here
    Parser_Timer.StopTimer();
    Logger::Log_Verbose("Parsing took %f milliseconds", "ViewportManager", 0, Parser_Timer.GetElapsedMilliseconds());
    FindTitle();

    Timer Layout_Timer;
    Layout_Timer.StartTimer();
    ApplyAndLayout();
    Layout_Timer.StopTimer();
    Logger::Log_Verbose("Layout took %f milliseconds", "ViewportManager", 0, Layout_Timer.GetElapsedMilliseconds());
}

void ViewportManager::SetLink(const std::string &Link) {
    CurrentLink = Link;
    LinkChanged = true;
}
bool NeedReconstruct(Node *dom) {
    if (dom->Reconstruct) {
        return true;
    }
    for (const auto& c : dom->children) {
        if (NeedReconstruct(c.get())) {
            return true;
        }
    }
    return false;
}
void ViewportManager::Update() {

    if (LinkChanged) {
        LinkChanged = false;
        if (tabContext) {
            engine.destroy_tab_context(tabContext);
            tabContext = engine.create_tab_context();

        }
        Init();

    }
    // check if reconstruct
    UpdateNeeded |= NeedReconstruct(&dom);

    if (UpdateNeeded) {
        UpdateNeeded = false;
        FindTitle();
        layout.Update(dom);
    }
}

void ViewportManager::Resize(int width, int height) {
    renderer.Resize(width, height);
    UpdateNeeded = true;
}
void ViewportManager::Step() {
    JavascriptFunctions::SetNewContext({&dom, });
    // Tell the engine to target this specific tab's runtime context before pumping events
    engine.set_active_context(tabContext);
    // ReSharper disable once CppExpressionWithoutSideEffects
    engine.Step();
}
std::vector<Color> ViewportManager::Render() {

    layoutRenderer.RenderRoot(layout.GetRoot());
    renderer.Present();
    return renderer.GetFrontBuffer();
}
// Assumes 'cache' is passed by reference or pointer from your ViewportManager context
void LoadImageResourceSync(Node& imgNode, const std::string& absoluteUrl, BrowserCacheManager& cache, int layoutWidth = 0 /* only for svg's */, int layoutHeight = 0) {
    // 1. Create the container immediately
    auto imgData = std::make_shared<ImageData>();
    imgNode.imageData = imgData;


    // 2. Grab raw web data
    std::string webData = cache.GetResource(absoluteUrl);
    if (webData.empty()) {
        std::cerr << "[Network Error] Failed to fetch image: " << absoluteUrl << std::endl;
        return;
    }

    // 3. Save to the local disk cache
    std::string localFilePath = ConvertUrlToCachePath(absoluteUrl);
    std::ofstream outFile(localFilePath, std::ios::binary);
    if (!outFile) {
        std::cerr << "[Disk Error] Could not write cache file: " << localFilePath << std::endl;
        return;
    }
    outFile.write(webData.data(), webData.size());
    outFile.close();

    // 4. Decode the file directly using your function
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rawBytes;
    int channels; // will always be 4
    bool isSvg = false;
    if (SvgViewer::IsSvg(std::vector<uint8_t>(webData.begin(), webData.end()))) {
        isSvg = true;
        SvgViewer viewer;
        if (layoutWidth == 0 || layoutHeight == 0) {
            viewer.GetIntrinsicDimensions(std::vector<uint8_t>(webData.begin(), webData.end()), layoutWidth, layoutHeight);
        }
        width = layoutWidth;
        height = layoutHeight;
        rawBytes = viewer.GetPixels(std::vector<uint8_t>(webData.begin(), webData.end()), layoutWidth, layoutHeight, channels );
        viewer.clean();
    } else {
        rawBytes = load_image(localFilePath, width, height, nullptr);
    }
    if (rawBytes.empty() || width <= 0 || height <= 0) {
        std::cerr << "[Decoder Error] load_image failed on: " << localFilePath << std::endl;
        return;
    }

    // 5. Unpack pixels into the vector
    imgData->intrinsicWidth = width;
    imgData->intrinsicHeight = height;
    imgData->pixels.reserve(width * height);

    for (size_t i = 0; i < rawBytes.size(); i += 4) {
        if (i + 3 < rawBytes.size()) {
            Color pixel;
            if (isSvg) {
                pixel.r = rawBytes[i];     // Byte 0
                pixel.g = rawBytes[i + 1]; // Byte 1
                pixel.b = rawBytes[i + 2]; // Byte 2
                pixel.a = rawBytes[i + 3]; // Byte 3
            } else {
                // Your existing configuration for load_image (if it returns BGRA)
                pixel.b = rawBytes[i];
                pixel.g = rawBytes[i + 1];
                pixel.r = rawBytes[i + 2];
                pixel.a = rawBytes[i + 3];
            }
            imgData->pixels.push_back(pixel);
        }
    }

    // Mark as ready. Since we're single-threaded, it's 100% ready before layout.Update runs.
    imgData->isLoaded = true;
}
void ViewportManager::ApplyAndLayout() {
    JavascriptFunctions::SetNewContext({&dom, &title});

    CSSParser cssParser;
    std::vector<CSSRule> rules;

    // ==========================================
    // 1. Collect Styles
    // ==========================================
    Timer Style_Collect_Timer;
    Style_Collect_Timer.StartTimer();

    std::function<void(Node&)> collectStyles = [&](Node& node) {
        auto it = node.attributes.find("rel");
        if (node.HasAttribute("style") || node.type == NodeType::Element && (node.tag == "style" || (it != node.attributes.end() && it->second == "stylesheet"))) {
            if (node.HasAttribute("style")) {
                auto parsed = cssParser.Parse(node.GetAttribute("style"), true);
                rules.insert(rules.end(), parsed.begin(), parsed.end());
            }

            if (it != node.attributes.end()) // link
            {
                auto ref = node.attributes.find("href");
                if (ref != node.attributes.end()) {
                    // 1. Resolve the href against the CurrentLink safely
                    std::string absoluteUrl = ResolveUrl(CurrentLink, ref->second);

                    // 2. Fetch using the properly formatted absolute URL
                    std::string res = cache.GetResource(absoluteUrl);

                    auto parsed = cssParser.Parse(res, false);
                    rules.insert(rules.end(), parsed.begin(), parsed.end());
                }
            }
            for (auto& child : node.children)
                if (child->type == NodeType::Text)
                    for (auto& rule : cssParser.Parse(child->text, false))
                        rules.push_back(rule);
        }
        for (auto& child : node.children)
            collectStyles(*child);
    };
    collectStyles(dom);

    Style_Collect_Timer.StopTimer();
    Logger::Log_Verbose("Style Collection took %f milliseconds", "ViewportManager", 1, Style_Collect_Timer.GetElapsedMilliseconds());

    // ==========================================
    // 2. Reset Styles
    // ==========================================
    Timer Style_Reset_Timer;
    Style_Reset_Timer.StartTimer();

    std::function<void(Node&)> resetStyles = [&](Node& node) {
        node.specifiedStyle = Style{};
        for (auto& child : node.children)
            resetStyles(*child);
    };
    resetStyles(dom);

    Style_Reset_Timer.StopTimer();
    Logger::Log_Verbose("Style Reset took %f milliseconds", "ViewportManager", 1, Style_Reset_Timer.GetElapsedMilliseconds());

    // ==========================================
    // 3. Apply CSS Rules
    // ==========================================
    Timer CSS_Apply_Timer;
    CSS_Apply_Timer.StartTimer();

    cssParser.Apply(rules, dom, renderer.GetWidth(), renderer.GetHeight());

    CSS_Apply_Timer.StopTimer();
    Logger::Log_Verbose("CSS Rule Application took %f milliseconds", "ViewportManager", 1, CSS_Apply_Timer.GetElapsedMilliseconds());

    // ==========================================
    // 4. Compute Style
    // ==========================================
    Timer Style_Compute_Timer;
    Style_Compute_Timer.StartTimer();

    // re-run style computation with fresh specifiedStyles
    ComputeStyle(dom); // you'll need to expose this from Parser

    Style_Compute_Timer.StopTimer();
    Logger::Log_Verbose("Style Computation took %f milliseconds", "ViewportManager", 1, Style_Compute_Timer.GetElapsedMilliseconds());

    // ==========================================
    // 5. Discover and Load Images
    // ==========================================
    Timer Image_Load_Timer;
    Image_Load_Timer.StartTimer();

    std::function<void(Node&)> discoverAndLoadImages = [&](Node& node) {
        if (node.type == NodeType::Element && node.tag == "img") {
            auto srcIt = node.attributes.find("src");
            auto widthIt = node.attributes.find("width");
            auto heightIt = node.attributes.find("height");
            int width = 0, height = 0;
            if (widthIt != node.attributes.end()) {
                width = std::stoi(widthIt->second);
            }
            if (heightIt != node.attributes.end()) {
                height = std::stoi(heightIt->second);
            }
            // Guard clause: Only trigger a fetch if a src exists AND we haven't
            // already allocated an image payload for this node.
            if (srcIt != node.attributes.end() && !node.imageData) {
                std::string absoluteUrl = ResolveUrl(CurrentLink, srcIt->second);

                // Call your asynchronous asset loader loader (from Step 4 previously)
                // This instantiates node.imageData immediately so layout doesn't crash
                LoadImageResourceSync(node, absoluteUrl, cache, width, height);
            }
        }
        for (auto& child : node.children) {
            discoverAndLoadImages(*child);
        }
    };
    discoverAndLoadImages(dom);

    Image_Load_Timer.StopTimer();
    Logger::Log_Verbose("Image Discovery & Sync Loading took %f milliseconds", "ViewportManager", 1, Image_Load_Timer.GetElapsedMilliseconds());

    // ==========================================
    // 6. Discover Javascript
    // ==========================================
    Timer JS_Discovery_Timer;
    JS_Discovery_Timer.StartTimer();

    std::function<void(Node&)> discoverAndRunJavascript = [&](Node& node) {
        if (node.type == NodeType::Element && node.tag == "script") {
            auto srcIt = node.attributes.find("src");
            if (srcIt != node.attributes.end()) {
                std::string absoluteUrl = ResolveUrl(CurrentLink, srcIt->second);
                std::string script = cache.GetResource(absoluteUrl);
                node.code = script;
                node.script_name = srcIt->second;
            }
            else {
                // Case B: Embedded Inline Script <script>console.log("hi");</script>
                std::string inlineScript = "";
                for (auto& child : node.children) {
                    if (child->type == NodeType::Text) {
                        inlineScript += child->text;
                    }
                }
                node.code = inlineScript;
                node.script_name = "inline";
            }
        }
        for (auto& child : node.children) {
            discoverAndRunJavascript(*child);
        }
    };
    discoverAndRunJavascript(dom);

    JS_Discovery_Timer.StopTimer();
    Logger::Log_Verbose("JavaScript Discovery & Setup took %f milliseconds", "ViewportManager", 1, JS_Discovery_Timer.GetElapsedMilliseconds());

    // ==========================================
    // 7. Layout Compilation
    // ==========================================
    Timer Layout_Compilation_Timer;
    Layout_Compilation_Timer.StartTimer();

    layout.Update(dom);
    layoutRenderer.UpdateDom(&dom);

    Layout_Compilation_Timer.StopTimer();
    Logger::Log_Verbose("Layout Compilation Update took %f milliseconds", "ViewportManager", 1, Layout_Compilation_Timer.GetElapsedMilliseconds());
}

std::vector<Color> ViewportManager::OnRender(int width, int height) {

    Step();
    Resize(width, height);
    Update();
    return Render();
}
void ViewportManager::RunNodeScripts(Node &node) {
    if (!node.code.empty()) {
        engine.Run(node.code, node.script_name);
    }
    for (auto &child : node.children) {
        RunNodeScripts(*child);
    }
}
void ViewportManager::StartScripts() {
    JavascriptFunctions::SetNewContext({&dom, &title});
    engine.set_active_context(tabContext);
RunNodeScripts(dom);
}
