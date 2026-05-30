//
// Created by tkdtu on 5/27/2026.
//

#include "Parser.h"

#include <iostream>
#include <ostream>
#include <stack>
#include <unordered_set>
#include <functional>
#include "CSS/CSSParser.h"

bool IsVoidElement(const std::string& tag) {
    static const std::unordered_set<std::string> voidTags = {
        "area",
        "base",
        "br",
        "col",
        "embed",
        "hr",
        "img",
        "input",
        "link",
        "meta",
        "source",
        "track",
        "wbr"
    };

    return voidTags.contains(tag);
}
std::string NormalizeText(const std::string& text)
{
    std::string out;
    bool prevSpace = false;

    for (char c : text)
    {
        bool isSpace = (c == ' ' || c == '\n' || c == '\t' || c == '\r');

        if (isSpace)
        {
            if (!prevSpace)
                out += ' ';
            prevSpace = true;
        }
        else
        {
            out += c;
            prevSpace = false;
        }
    }

    return out;
}
void NormalizeDOM(Node& root)
{
    bool hasHtml = false;
    for (auto& c : root.children)
    {
        if (c->tag == "html")
            hasHtml = true;
    }

    if (!hasHtml)
    {
        auto html = std::make_unique<Node>();
        html->type = NodeType::Element;
        html->tag = "html";

        // Keep a temporary list for things that stay outside <html> (like DOCTYPE)
        std::vector<std::unique_ptr<Node>> outsideNodes;

        for (auto& c : root.children) {
            if (c->type == NodeType::Doctype || c->type == NodeType::Comment) {
                // Keep doctypes and top-level comments attached directly to Document
                outsideNodes.push_back(std::move(c));
            } else {
                // Everything else (body, tags, text) gets wrapped by <html>
                html->children.push_back(std::move(c));
            }
        }

        root.children.clear();

        // Re-assign top level nodes cleanly back to root
        for (auto& node : outsideNodes) {
            root.children.push_back(std::move(node));
        }

        // Append the wrapped html subtree as a sibling next to the DOCTYPE
        root.children.push_back(std::move(html));
    }
}
// UA defaults. Members left at Style's defaults mean "this tag doesn't set
// this property", so inherited values are preserved.
//
// font_size: 0 means "inherit from parent" (Style's default is 16, but we
// override the constructed Style to 0 below for tags that don't fix a size).
Style DefaultStyleForTag(const std::string& tag) {
    Style s;

    // Default fallback unit state across the board
    s.font_size     = { 0.0f, LengthUnit::Inherit }; // Matches old 0 sentinel "inherit"
    s.margin_left   = { 0.0f, LengthUnit::Px };
    s.margin_right  = { 0.0f, LengthUnit::Px };
    s.margin_top    = { 0.0f, LengthUnit::Px };
    s.margin_bottom = { 0.0f, LengthUnit::Px };
    s.padding_left  = { 0.0f, LengthUnit::Px };


    if (tag == "p") {
        s.margin_top    = { 16.0f, LengthUnit::Px };
        s.margin_bottom = { 16.0f, LengthUnit::Px };
    }
    else if (tag == "body") {
        s.font_size     = { 16.0f, LengthUnit::Px };
        s.margin_left   = { 8.0f, LengthUnit::Px };
        s.margin_right  = { 8.0f, LengthUnit::Px };
        s.margin_top    = { 8.0f, LengthUnit::Px };
        s.margin_bottom = { 8.0f, LengthUnit::Px };
    }
    else if (tag == "div") {
        s.font_size     = { 0.0f, LengthUnit::Inherit };
    }
    else if (tag == "h1") {
        s.font_size     = { 2.0f, LengthUnit::Em };
        s.margin_top    = { 21.0f, LengthUnit::Px };
        s.margin_bottom = { 21.0f, LengthUnit::Px };
    }
    else if (tag == "h2") {
        s.font_size     = { 1.5f, LengthUnit::Em };
        s.margin_top    = { 19.0f, LengthUnit::Px };
        s.margin_bottom = { 19.0f, LengthUnit::Px };
    }
    else if (tag == "h3") {
        s.font_size     = { 1.17f, LengthUnit::Em };
        s.margin_top    = { 18.0f, LengthUnit::Px };
        s.margin_bottom = { 18.0f, LengthUnit::Px };
    }
    else if (tag == "h4") {
        s.font_size     = { 16.0f, LengthUnit::Px };
        s.margin_top    = { 21.0f, LengthUnit::Px };
        s.margin_bottom = { 21.0f, LengthUnit::Px };
    }
    else if (tag == "h5") {
        s.font_size     = { 13.0f, LengthUnit::Px };
        s.margin_top    = { 22.0f, LengthUnit::Px };
        s.margin_bottom = { 22.0f, LengthUnit::Px };
    }
    else if (tag == "h6") {
        s.font_size     = { 11.0f, LengthUnit::Px };
        s.margin_top    = { 24.0f, LengthUnit::Px };
        s.margin_bottom = { 24.0f, LengthUnit::Px };
    }
    else if (tag == "ul" || tag == "ol") {
        s.font_size     = { 13.0f, LengthUnit::Px };
        s.margin_top    = { 16.0f, LengthUnit::Px };
        s.margin_bottom = { 16.0f, LengthUnit::Px };
        s.padding_left  = { 40.0f, LengthUnit::Px };
    }

    return s;
}
static void ApplyAttributes(Node& node)
{
    auto& a = node.attributes;

    if (a.contains("id")) {
        node.id = a["id"];
    }
    if (a.contains("class")) {
        node.class_name = a["class"];
    }
}

void ComputeStyle(Node& node, const Style* parentStyle) {
    Style spec = node.specifiedStyle;
    Style tagDefaults = DefaultStyleForTag(node.tag);
    Style result;

    // --- Font Size Cascade ---
    if (spec.set.font_size) {
        result.set.font_size = true;
        result.font_size = spec.font_size;
    } else if (tagDefaults.font_size.unit != LengthUnit::Inherit) {
        result.font_size = tagDefaults.font_size;
    } else if (parentStyle) {
        result.font_size = parentStyle->font_size;
    } else {
        result.font_size = { 16.0f, LengthUnit::Px }; // Final absolute fallback
    }

    // --- Margin Auto Layout Cascade ---
    if (spec.set.margin_left && spec.margin_left.unit == LengthUnit::Auto) {
        result.margin_left = { 0.0f, LengthUnit::Auto };
    } else {
        result.margin_left = spec.set.margin_left ? spec.margin_left : tagDefaults.margin_left;
    }

    if (spec.set.margin_right && spec.margin_right.unit  == LengthUnit::Auto) {
        result.margin_right = { 0.0f, LengthUnit::Auto };
    } else {
        result.margin_right = spec.set.margin_right ? spec.margin_right : tagDefaults.margin_right;
    }

    // --- Standard Dimensions/Paddings Assignments ---
    result.margin_top    = spec.set.margin_top ? spec.margin_top : tagDefaults.margin_top;
    result.margin_bottom = spec.set.margin_bottom ? spec.margin_bottom : tagDefaults.margin_bottom;
    result.padding_left  = spec.set.padding_left ? spec.padding_left : tagDefaults.padding_left;
    // Fix: Add missing margins and paddings!
    result.padding_right  = spec.set.padding_right ? spec.padding_right : tagDefaults.padding_right;
    result.padding_top    = spec.set.padding_top ? spec.padding_top : tagDefaults.padding_top;
    result.padding_bottom = spec.set.padding_bottom ? spec.padding_bottom : tagDefaults.padding_bottom;

    // Fix: Copy Dimensions (Required for block centering)
    result.width = spec.width;
    result.height = spec.height;
    result.min_height = spec.min_height;
    result.max_height = spec.max_height;
    result.boxSizing = spec.boxSizing;

    // Fix: Copy Borders (Required to see them)
    result.BorderTop = spec.BorderTop;
    result.BorderBottom = spec.BorderBottom;
    result.BorderLeft = spec.BorderLeft;
    result.BorderRight = spec.BorderRight;

    // Fix: Copy Typography & Backgrounds (Required for text centering)
    if (spec.set.textAlign) { // ensure you track if it was explicitly parsed
        result.textAlign = spec.textAlign;
    } else if (parentStyle) {
        result.textAlign = parentStyle->textAlign;
    } else {
        result.textAlign = tagDefaults.textAlign; // usually Left
    }
    result.color = spec.color;
    result.hasBackground = spec.hasBackground;
    result.backgroundColor = spec.backgroundColor;
    result.font_bold = spec.font_bold;
    result.font_italic = spec.font_italic;

    result.textDecoration = spec.textDecoration;
    result.textDecorationStyle = spec.textDecorationStyle;
    result.TextDecorationColor = spec.TextDecorationColor;
    result.TextDecorationThickness = spec.TextDecorationThickness;

    result.whiteSpace = spec.whiteSpace;
    result.textOverflow = spec.textOverflow;

    // Save the computed style map safely back onto the DOM tracking node
    node.computedStyle = result;

    for (auto &child : node.children)
    {
        ComputeStyle(*child, &node.computedStyle);
    }
}
Node Parser::Parse(const std::vector<Token>& tokens) {

    Node root;
    root.type = NodeType::Document;

    std::stack<Node*> nodeStack;

    nodeStack.push(&root);

for (const Token& token : tokens) {
    std::cout << "Token: " << (int)token.type << " | Value: " << token.value << std::endl;
    switch (token.type) {
        case TokenType::Doctype: {
            auto node = std::make_unique<Node>();
            node->type = NodeType::Doctype;
            node->tag = token.value;
            node->parent = nodeStack.top(); // This points to 'root' (Document)
            node->attributes = token.attributes;
            ApplyAttributes(*node);

            // Attached directly to Document root.
            // Crucial: We do NOT push this onto nodeStack!
            nodeStack.top()->children.push_back(std::move(node));
            break;
        }
        case TokenType::OpenTag: {
            auto node = std::make_unique<Node>();
            node->type = NodeType::Element;
            node->tag = token.value;
            node->parent = nodeStack.top();
            node->attributes = token.attributes;

            Node* rawPtr = node.get();
            ApplyAttributes(*node);
            nodeStack.top()->children.push_back(std::move(node));

            // ONLY push non-void elements onto the stack to become parents
            if (!IsVoidElement(token.value)) {
                nodeStack.push(rawPtr);
            }
            break;
        }
        case TokenType::Text: {
            auto node = std::make_unique<Node>();
            node->type = NodeType::Text;
            node->text = NormalizeText(token.value);
            node->parent = nodeStack.top();
            node->attributes = token.attributes;
            ApplyAttributes(*node);
            nodeStack.top()->children.push_back(std::move(node));
            break;
        }
        case TokenType::Comment: {
            auto node = std::make_unique<Node>();
            node->type = NodeType::Comment;
            node->text = token.value;
            node->parent = nodeStack.top();
            ApplyAttributes(*node);
            nodeStack.top()->children.push_back(std::move(node));
            break; // <--- FIX: This was missing! Without this, it fell through to CloseTag
        }
        case TokenType::CloseTag: {
            if (nodeStack.size() <= 1) break;

            const std::string& closingTag = token.value;
            std::vector<Node*> toRestore;

            while (nodeStack.size() > 1) {
                Node* top = nodeStack.top();
                nodeStack.pop();

                if (top->tag == closingTag) break;
                toRestore.push_back(top);
            }

            for (int i = (int)toRestore.size() - 1; i >= 0; i--) {
                nodeStack.push(toRestore[i]);
            }
            break;
        }
        default:
            std::cout << "Unknown token type" << std::endl;
            break;
    }
}
    NormalizeDOM(root);

    return root;
}


void Parser::PrintNode(const Node& node, int depth) {

    // indentation
    for (int i = 0; i < depth; i++) {
        std::cout << "  ";
    }

    switch (node.type) {
        case NodeType::Doctype:
            std::cout << "DOCTYPE: ";
            for (const auto& [key, value] : node.attributes) {
                std::cout << key;
            }
            break;
        case NodeType::Document:
            std::cout << "Document";
            break;

        case NodeType::Element:
            std::cout << "<" << node.tag;

            for (const auto& [key, value] : node.attributes) {
                std::cout << " " << key << "=\"" << value << "\"";
            }

            std::cout << ">";
            break;

        case NodeType::Text:
            std::cout << "TEXT: \"" << node.text << "\"";
            break;
    }

    std::cout << '\n';

    // print children recursively
    for (const auto& child : node.children) {
        PrintNode(*child, depth + 1);
    }
}

Node * Parser::FindNodeByTag(Node *dom, std::string tag) {
    if (dom->tag  == tag) {
        return dom;
    }
    for (auto& child : dom->children) {
        if (auto found = FindNodeByTag(child.get(), tag)) {
            return found;
        }
    }
    return nullptr;
}
