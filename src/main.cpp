#include <functional>
#include <iostream>


#include "CurlGrabber.h"
#include "Tokenizer.h"
#include "Parser.h"
#include "CSS/CSSParser.h"
#include "Platform/Platform.h"
#include "Render/Renderer.h"
#include "Layout/LayoutRenderer.h"
int main() {

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

    Grab response = grabber.GetData("https://www.example.com");
    std::cout << response.data << std::endl;

    Tokenizer tokenizer;
    auto tokens = tokenizer.tokenize(response.data);
    // parse once
    Parser parser;
    auto dom = parser.Parse(tokens); // no renderer dependency here
    parser.PrintNode(dom);

    // apply CSS + layout separately so both can be re-run on resize
    Font font("arial/ARIAL.TTF", 32);
    LayoutRenderer layout(renderer, font);

    auto applyAndLayout = [&]() {
        // re-apply CSS with current viewport size
        CSSParser cssParser;
        std::vector<CSSRule> rules;
        std::function<void(Node&)> collectStyles = [&](Node& node) {
            if (node.type == NodeType::Element && node.tag == "style") {
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

    while (platform->IsRunning()) {
        Event event;
        while (platform->PollEvent(event)) {
            if (event.type == EventType::Resize) {
                renderer.Resize(event.width, event.height);
                applyAndLayout();
            }
        }

        layout.Render();
        renderer.Present();
        platform->Present(renderer.GetFrontBuffer());
    }
    return 0;
}
