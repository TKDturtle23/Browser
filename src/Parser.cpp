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

        html->children = std::move(root.children);
        root.children.clear();
        root.children.push_back(std::move(html));
    }
}
// UA defaults. Members left at Style's defaults mean "this tag doesn't set
// this property", so inherited values are preserved.
//
// font_size: 0 means "inherit from parent" (Style's default is 16, but we
// override the constructed Style to 0 below for tags that don't fix a size).
static Style DefaultStyleForTag(const std::string& tag)
{
    Style s;
    s.font_size = 0; // sentinel: "not set by this tag, inherit"
    s.backgroundColor = Color(255, 255, 255);
    s.hasBackground = false;
    s.margin_left_auto = false;
    s.margin_right_auto = false;
    if (tag == "div")
    {
        s.display = DisplayType::Block;
    }
    else if (tag == "p")
    {
        s.display = DisplayType::Block;
        s.margin_top = 8;
        s.margin_bottom = 8;
    }
    else if (tag == "body")
    {
        s.display = DisplayType::Block;
        s.font_size = 16;
        s.margin_left = 8;
        s.margin_right = 8;
        s.margin_top = 8;
        s.margin_bottom = 8;
    }
    else if (tag == "html")
    {
        s.display = DisplayType::Block;
        s.font_size = 16;
    }
    else if (tag == "h1")
    {
        s.display = DisplayType::Block;
        s.font_size = 32;
        s.font_bold = true;
        s.margin_top = 21;
        s.margin_bottom = 21;
    }
    else if (tag == "h2")
    {
        s.display = DisplayType::Block;
        s.font_size = 24;
        s.font_bold = true;
        s.margin_top = 19;
        s.margin_bottom = 19;
    }
    else if (tag == "h3")
    {
        s.display = DisplayType::Block;
        s.font_size = 18;
        s.font_bold = true;
        s.margin_top = 18;
        s.margin_bottom = 18;
    }
    else if (tag == "h4")
    {
        s.display = DisplayType::Block;
        s.font_size = 16;
        s.font_bold = true;
        s.margin_top = 21;
        s.margin_bottom = 21;
    }
    else if (tag == "h5")
    {
        s.display = DisplayType::Block;
        s.font_size = 13;
        s.font_bold = true;
        s.margin_top = 22;
        s.margin_bottom = 22;
    }
    else if (tag == "h6")
    {
        s.display = DisplayType::Block;
        s.font_size = 11;
        s.font_bold = true;
        s.margin_top = 24;
        s.margin_bottom = 24;
    }
    else if (tag == "b" || tag == "strong")
    {
        s.font_bold = true;
    }
    else if (tag == "i" || tag == "em")
    {
        s.font_italic = true;
    }
    else if (tag == "small")
    {
        s.font_size = 13;
    }
    else if (tag == "ul" || tag == "ol")
    {
        s.display = DisplayType::Block;
        s.margin_top = 8;
        s.margin_bottom = 8;
        s.padding_left = 40;
    }
    else if (tag == "li")
    {
        s.display = DisplayType::Block;
    }

    return s;
}

static void ApplyAttributes(Node& node)
{
    auto& a = node.attributes;

    if (a.contains("display"))
    {
        if (a["display"] == "block")
            node.specifiedStyle.display = DisplayType::Block;
        else if (a["display"] == "inline")
            node.specifiedStyle.display = DisplayType::Inline;
    }

    if (a.contains("font-size"))
    {
        node.specifiedStyle.font_size = std::stoi(a["font-size"]);
    }
}

void ComputeStyle(Node& node, const Style* parentStyle)
{
    Style tagDefaults = DefaultStyleForTag(node.tag);
    Style result;

    // 1. Inherit font properties from parent (these are the CSS-inherited ones).
    if (parentStyle)
    {
        result.font_family = parentStyle->font_family;
        result.font_size   = parentStyle->font_size;
        result.font_bold   = parentStyle->font_bold;
        result.font_italic = parentStyle->font_italic;
        result.textAlign = parentStyle->textAlign;
    }

    // 2. Apply UA defaults for the tag. Inherited properties only overwrite
    //    when the tag actually sets them; layout properties (margin/padding/
    //    display) overwrite unconditionally since they don't inherit.
    if (tagDefaults.font_size != 0) result.font_size   = tagDefaults.font_size;
    if (tagDefaults.font_bold)      result.font_bold   = true;
    if (tagDefaults.font_italic)    result.font_italic = true;

    result.display       = tagDefaults.display;
    result.position      = tagDefaults.position;
    result.margin_top    = tagDefaults.margin_top;
    result.margin_bottom = tagDefaults.margin_bottom;
    result.margin_left   = tagDefaults.margin_left;
    result.margin_right  = tagDefaults.margin_right;
    result.padding_top   = tagDefaults.padding_top;
    result.padding_bottom = tagDefaults.padding_bottom;
    result.padding_left  = tagDefaults.padding_left;
    result.padding_right = tagDefaults.padding_right;
    result.width  = tagDefaults.width;
    result.height = tagDefaults.height;
    result.margin_left_auto  = tagDefaults.margin_left_auto;
    result.margin_right_auto = tagDefaults.margin_right_auto;
// 3. Apply author-specified styles (highest priority).
ApplyAttributes(node);

if (node.specifiedStyle.font_size != 0)
    result.font_size = node.specifiedStyle.font_size;

// resolve em units now that parent font_size is known
if (node.specifiedStyle.font_size_em > 0.0f) {
    int base = parentStyle ? parentStyle->font_size : 16;
    result.font_size = static_cast<int>(node.specifiedStyle.font_size_em * base);
}
if (node.specifiedStyle.display != DisplayType::Inline)
    result.display = node.specifiedStyle.display;

if (node.specifiedStyle.font_bold)
    result.font_bold = true;
if (node.specifiedStyle.textAlign != TextAlign::Left)
    result.textAlign = node.specifiedStyle.textAlign;
if (node.specifiedStyle.font_italic)
    result.font_italic = true;

// color inherits, so only override if explicitly set
if (node.specifiedStyle.color.r != 0
 || node.specifiedStyle.color.g != 0
 || node.specifiedStyle.color.b != 0)
{
    result.color = node.specifiedStyle.color;
}

// background does not inherit
if (node.specifiedStyle.hasBackground) {
    result.hasBackground   = true;
    result.backgroundColor = node.specifiedStyle.backgroundColor;
}

// border does not inherit
if (node.specifiedStyle.border.any()) {
    result.border = node.specifiedStyle.border;
}

// layout properties — only override if author explicitly set them
// (non-zero is our sentinel for "was set", same pattern as font_size)
if (node.specifiedStyle.margin_top    != 0) result.margin_top    = node.specifiedStyle.margin_top;
if (node.specifiedStyle.margin_bottom != 0) result.margin_bottom = node.specifiedStyle.margin_bottom;
if (node.specifiedStyle.margin_left   != 0) result.margin_left   = node.specifiedStyle.margin_left;
if (node.specifiedStyle.margin_right  != 0) result.margin_right  = node.specifiedStyle.margin_right;
if (node.specifiedStyle.padding_top    != 0) result.padding_top    = node.specifiedStyle.padding_top;
if (node.specifiedStyle.padding_bottom != 0) result.padding_bottom = node.specifiedStyle.padding_bottom;
if (node.specifiedStyle.padding_left   != 0) result.padding_left   = node.specifiedStyle.padding_left;
if (node.specifiedStyle.padding_right  != 0) result.padding_right  = node.specifiedStyle.padding_right;
if (node.specifiedStyle.width  != -1) result.width  = node.specifiedStyle.width;
if (node.specifiedStyle.height != -1) result.height = node.specifiedStyle.height;
if (node.specifiedStyle.margin_left_auto) {
    result.margin_left = 0;
    result.margin_left_auto = true;
} else if (node.specifiedStyle.margin_left != 0) {
    result.margin_left = node.specifiedStyle.margin_left;
}

if (node.specifiedStyle.margin_right_auto) {
    result.margin_right = 0;
    result.margin_right_auto = true;
} else if (node.specifiedStyle.margin_right != 0) {
    result.margin_right = node.specifiedStyle.margin_right;
}

    node.computedStyle = result;
    // 4. Recurse.
    for (auto& child : node.children)
    {
        child->parent = &node;
        ComputeStyle(*child, &node.computedStyle);
    }
}

Node Parser::Parse(const std::vector<Token>& tokens) {

    Node root;
    root.type = NodeType::Document;

    std::stack<Node*> nodeStack;

    nodeStack.push(&root);

    for (const Token& token : tokens) {

        switch (token.type) {
            case TokenType::Doctype: {

                auto node = std::make_unique<Node>();

                node->type = NodeType::Doctype;
                node->text = token.value;
                node->parent = nodeStack.top();
                node->attributes = token.attributes;
                nodeStack.top()->children.push_back(
                    std::move(node)
                );

                break;
            }
            case TokenType::OpenTag: {

                auto node = std::make_unique<Node>();

                node->type = NodeType::Element;
                node->tag = token.value;
                node->parent = nodeStack.top();
                node->attributes = token.attributes;
                Node* rawPtr = node.get();

                nodeStack.top()->children.push_back(
                    std::move(node)
                );

                // ONLY push non-void elements
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

                nodeStack.top()->children.push_back(
                    std::move(node)
                );

                break;
            }

            case TokenType::CloseTag: {
                if (nodeStack.size() <= 1) break;

                const std::string& closingTag = token.value;

                // Search for matching tag without disturbing the stack first
                // Walk a copy to find it
                std::vector<Node*> toRestore;

                while (nodeStack.size() > 1) {
                    Node* top = nodeStack.top();
                    nodeStack.pop();

                    if (top->tag == closingTag) break;  // found it, discard it
                    toRestore.push_back(top);           // unclosed tag above match
                }

                // Re-push in original order (toRestore is reversed, so iterate backwards)
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