#include <functional>
#include <iostream>


#include "CurlGrabber.h"
#include "Tokenizer.h"
#include "Parser.h"
#include "CSS/CSSParser.h"
#include "Platform/Platform.h"
#include "Render/Renderer.h"
#include "Layout/LayoutRenderer.h"
#include "Platform/Platform_win32.h"

int main() {
    std::string link = "https://example.com"; // link should have a / at the end
    auto platform =
        CreatePlatform();

    if (!platform->OpenWindow(
        800,
        600,
        "Browser"
    )) {
        return 1;
    }

    Renderer renderer(
        800,
        600
    );


    CurlGrabber grabber;
    grabber.Init();

    Grab response = grabber.GetData(link);
    std::cout << response.data << std::endl;

    Tokenizer tokenizer;
    auto tokens = tokenizer.tokenize(response.data);
    // parse once
    Parser parser;
    auto dom = parser.Parse(tokens); // no renderer dependency here
    parser.PrintNode(dom);

    // apply CSS + layout separately so both can be re-run on resize

    LayoutRenderer layout(renderer);

    auto applyAndLayout = [&]() {
        // re-apply CSS with current viewport size
        CSSParser cssParser;
        std::vector<CSSRule> rules;
        std::function<void(Node&)> collectStyles = [&](Node& node) {
            auto it = node.attributes.find("rel");
            if (node.type == NodeType::Element && (node.tag == "style" || (it != node.attributes.end() && it->second == "stylesheet"))) {
                if (it != node.attributes.end()) // link
                {
                    auto ref = node.attributes.find("href");
                    if (ref != node.attributes.end()) {
                        std::cout << "Loading " << ref->second << std::endl;
                        auto response = grabber.GetData(link + ref->second);
                        auto parsed = cssParser.Parse(response.data);
                        rules.insert(rules.end(), parsed.begin(), parsed.end());
                    }

                }
                for (auto& child : node.children)
                    if (child->type == NodeType::Text)
                        for (auto& rule : cssParser.Parse(child->text))
                            rules.push_back(rule);
            }
            for (auto& child : node.children)
                collectStyles(*child);
        };
        collectStyles(dom);
        std::function<void(Node&)> resetStyles = [&](Node& node) {
            node.specifiedStyle = Style{};
            for (auto& child : node.children)
                resetStyles(*child);
        };
        resetStyles(dom);
        cssParser.Apply(rules, dom, renderer.GetWidth(), renderer.GetHeight());

        // re-run style computation with fresh specifiedStyles
        ComputeStyle(dom); // you'll need to expose this from Parser

        layout.Update(dom);
    };

    applyAndLayout();
    platform->onRender = [&]() {
        renderer.Resize(platform->GetWidth(), platform->GetHeight());
        applyAndLayout();

        layout.Render();
        renderer.Present();
        platform->Present(renderer.GetFrontBuffer());
    };
    while (platform->IsRunning()) {

        Event event;
        while (platform->PollEvent(event)) {
            if (event.type == EventType::Resize) {
                renderer.Resize(event.width, event.height);
                applyAndLayout();
                std::cout << "Resized to " << event.width << "x" << event.height << std::endl;
            }
        }

        if (platform->needsRedraw) {
            platform->needsRedraw = false;

            layout.Render();
            renderer.Present();
            platform->Present(renderer.GetFrontBuffer());
        }

        // keep message pump alive
        Sleep(0);

    }
    return 0;
}
