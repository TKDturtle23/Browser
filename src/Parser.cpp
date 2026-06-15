
#include "Parser.h"

#include <iostream>
#include <ostream>
#include <stack>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <sstream>

#include "CSS/CSSParser.h"

bool IsVoidElement(const std::string& tag) {
    static const std::unordered_set<std::string> voidTags = {
        "area","base","br","col","embed","hr","img",
        "input","link","meta","source","track","wbr"
    };

    return voidTags.contains(tag);
}

static bool IsWhitespace(char c)
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

std::string NormalizeText(const std::string& text)
{
    std::string out;
    bool prevSpace = false;

    for (char c : text)
    {
        bool isSpace = IsWhitespace(c);

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

static bool IsAllWhitespace(const std::string& text)
{
    return std::all_of(text.begin(), text.end(),
        [](char c) { return IsWhitespace(c); });
}

void NormalizeDOM(Node& root)
{
    bool hasHtml = false;

    for (auto& c : root.children) {
        if (c->tag == "html") {
            hasHtml = true;
            break;
        }
    }

    if (!hasHtml)
    {
        auto html = std::make_unique<Node>();
        html->type = NodeType::Element;
        html->tag = "html";
        html->parent = &root;

        auto head = std::make_unique<Node>();
        head->type = NodeType::Element;
        head->tag = "head";
        head->parent = html.get();

        auto body = std::make_unique<Node>();
        body->type = NodeType::Element;
        body->tag = "body";
        body->parent = html.get();

        for (auto& c : root.children)
        {
            if (!c)
                continue;

            if (c->type == NodeType::Doctype ||
                c->type == NodeType::Comment)
            {
                continue;
            }

            if (c->tag == "title" ||
                c->tag == "meta" ||
                c->tag == "link" ||
                c->tag == "style")
            {
                c->parent = head.get();
                head->children.push_back(std::move(c));
            }
            else
            {
                c->parent = body.get();
                body->children.push_back(std::move(c));
            }
        }

        root.children.clear();

        html->children.push_back(std::move(head));
        html->children.push_back(std::move(body));

        root.children.push_back(std::move(html));
    }
}

static void MarkSet(Style& s)
{
    s.set.font_size = true;
    s.set.margin_left = true;
    s.set.margin_right = true;
    s.set.margin_top = true;
    s.set.margin_bottom = true;
    s.set.padding_left = true;
}

Style DefaultStyleForTag(const std::string& tag)
{
    Style s;

    s.font_size     = { 0.0f, LengthUnit::Inherit };
    s.margin_left   = { 0.0f, LengthUnit::Px };
    s.margin_right  = { 0.0f, LengthUnit::Px };
    s.margin_top    = { 0.0f, LengthUnit::Px };
    s.margin_bottom = { 0.0f, LengthUnit::Px };
    s.padding_left  = { 0.0f, LengthUnit::Px };


    if (tag == "body")
    {
        s.font_size     = { 16.0f, LengthUnit::Px };
        s.set.font_size = true;

        s.margin_left   = { 8.0f, LengthUnit::Px };
        s.set.margin_left = true;

        s.margin_right  = { 8.0f, LengthUnit::Px };
        s.set.margin_right = true;

        s.margin_top    = { 8.0f, LengthUnit::Px };
        s.set.margin_top = true;

        s.margin_bottom = { 8.0f, LengthUnit::Px };
        s.set.margin_bottom = true;
    }
    else if (tag == "p")
    {
        s.margin_top    = { 16.0f, LengthUnit::Px };
        s.set.margin_top = true;

        s.margin_bottom = { 16.0f, LengthUnit::Px };
        s.set.margin_bottom = true;
    }
    else if (tag == "h1")
    {
        s.font_size     = { 2.0f, LengthUnit::Em };
        s.set.font_size = true;

        s.margin_top    = { 21.0f, LengthUnit::Px };
        s.set.margin_top = true;

        s.margin_bottom = { 21.0f, LengthUnit::Px };
        s.set.margin_bottom = true;
    }
    else if (tag == "h2")
    {
        s.font_size     = { 1.5f, LengthUnit::Em };
        s.set.font_size = true;

        s.margin_top    = { 19.0f, LengthUnit::Px };
        s.set.margin_top = true;

        s.margin_bottom = { 19.0f, LengthUnit::Px };
        s.set.margin_bottom = true;
    }
    else if (tag == "ul" || tag == "ol")
    {
        s.padding_left  = { 40.0f, LengthUnit::Px };
        s.set.padding_left = true;

        s.margin_top    = { 16.0f, LengthUnit::Px };
        s.set.margin_top = true;

        s.margin_bottom = { 16.0f, LengthUnit::Px };
        s.set.margin_bottom = true;
    }

    return s;
}

static bool IsInheritedProperty_Display()
{
    return false;
}

static void ApplyAttributes(Node& node)
{
    auto& a = node.attributes;

    if (a.contains("id"))
        node.id = a["id"];

    if (a.contains("class"))
    {
        node.class_name = a["class"];

        node.classList.clear();

        std::stringstream ss(a["class"]);
        std::string token;

        while (ss >> token)
            node.classList.push_back(token);
    }
}

template<typename T>
static void ResolveProperty(
    T& outValue,
    bool& outSet,
    const T& specValue,
    bool specSet,
    const T& defaultValue,
    bool defaultSet,
    const T* parentValue,
    bool inheritedProperty)
{
    if (specSet)
    {
        outValue = specValue;
        outSet = true;
    }
    else if (defaultSet)
    {
        outValue = defaultValue;
        outSet = true;
    }
    else if (inheritedProperty && parentValue)
    {
        outValue = *parentValue;
        outSet = true;
    }
}

void ComputeStyle(Node& node, const Style* parentStyle)
{
    Style result;

    const Style& spec = node.specifiedStyle;
    Style defaults = DefaultStyleForTag(node.tag);

    // ============================================================
    // 1. INHERITED / RESOLVED PROPERTIES
    // ============================================================

    ResolveProperty(
        result.font_size,
        result.set.font_size,
        spec.font_size,
        spec.set.font_size,
        defaults.font_size,
        defaults.set.font_size,
        parentStyle ? &parentStyle->font_size : nullptr,
        true
    );

    ResolveProperty(
        result.color,
        result.set.color,
        spec.color,
        spec.set.color,
        defaults.color,
        defaults.set.color,
        parentStyle ? &parentStyle->color : nullptr,
        true
    );

    ResolveProperty(
        result.display,
        result.set.display,
        spec.display,
        spec.set.display,
        defaults.display,
        defaults.set.display,
        parentStyle ? &parentStyle->display : nullptr,
        false // display does not inherit by default
    );

    ResolveProperty(
        result.textAlign,
        result.set.textAlign,
        spec.textAlign,
        spec.set.textAlign,
        defaults.textAlign,
        defaults.set.textAlign,
        parentStyle ? &parentStyle->textAlign : nullptr,
        true
    );

    ResolveProperty(
        result.whiteSpace,
        result.set.whiteSpace,
        spec.whiteSpace,
        spec.set.whiteSpace,
        defaults.whiteSpace,
        defaults.set.whiteSpace,
        parentStyle ? &parentStyle->whiteSpace : nullptr,
        true
    );

    ResolveProperty(
        result.textOverflow,
        result.set.textOverflow,
        spec.textOverflow,
        spec.set.textOverflow,
        defaults.textOverflow,
        defaults.set.textOverflow,
        parentStyle ? &parentStyle->textOverflow : nullptr,
        false
    );

    ResolveProperty(
        result.overflow,
        result.set.overflow,
        spec.overflow,
        spec.set.overflow,
        defaults.overflow,
        defaults.set.overflow,
        parentStyle ? &parentStyle->overflow : nullptr,
        false
    );

    ResolveProperty(
        result.boxSizing,
        result.set.boxSizing,
        spec.boxSizing,
        spec.set.boxSizing,
        defaults.boxSizing,
        defaults.set.boxSizing,
        parentStyle ? &parentStyle->boxSizing : nullptr,
        false
    );

    ResolveProperty(
        result.objectFit,
        result.set.objectFit,
        spec.objectFit,
        spec.set.objectFit,
        defaults.objectFit,
        defaults.set.objectFit,
        parentStyle ? &parentStyle->objectFit : nullptr,
        false
    );

    ResolveProperty(
        result.verticalAlign,
        result.set.verticalAlign,
        spec.verticalAlign,
        spec.set.verticalAlign,
        defaults.verticalAlign,
        defaults.set.verticalAlign,
        parentStyle ? &parentStyle->verticalAlign : nullptr,
        false
    );

    // ============================================================
    // 2. NON-INHERITED / DIRECT PROPERTIES
    // ============================================================

    // Layout & Positioning
    result.position = spec.position;

    // Margins
    result.margin_top    = spec.set.margin_top    ? spec.margin_top    : defaults.margin_top;
    result.margin_bottom = spec.set.margin_bottom ? spec.margin_bottom : defaults.margin_bottom;
    result.margin_left   = spec.set.margin_left   ? spec.margin_left   : defaults.margin_left;
    result.margin_right  = spec.set.margin_right  ? spec.margin_right  : defaults.margin_right;

    // Padding
    result.padding_top    = spec.set.padding_top    ? spec.padding_top    : defaults.padding_top;
    result.padding_bottom = spec.set.padding_bottom ? spec.padding_bottom : defaults.padding_bottom;
    result.padding_left   = spec.set.padding_left   ? spec.padding_left   : defaults.padding_left;
    result.padding_right  = spec.set.padding_right  ? spec.padding_right  : defaults.padding_right;

    result.border_radius_top_left    = spec.set.border_radius_top_left    ? spec.border_radius_top_left    : defaults.border_radius_top_left;
    result.border_radius_top_right = spec.set.border_radius_top_right ? spec.border_radius_top_right : defaults.border_radius_top_right;
    result.border_radius_bottom_right   = spec.set.border_radius_bottom_right   ? spec.border_radius_bottom_right   : defaults.border_radius_bottom_right;
    result.border_radius_bottom_left  = spec.set.border_radius_bottom_left  ? spec.border_radius_bottom_left  : defaults.border_radius_bottom_left;

    // Dimensions
    result.width      = spec.set.width      ? spec.width      : defaults.width;
    result.height     = spec.set.height     ? spec.height     : defaults.height;
    result.min_width  = spec.set.min_width  ? spec.min_width  : defaults.min_width;
    result.max_width  = spec.set.max_width  ? spec.max_width  : defaults.max_width;
    result.min_height = spec.set.min_height ? spec.min_height : defaults.min_height;
    result.max_height = spec.set.max_height ? spec.max_height : defaults.max_height;

    // Typography & Decorations
    result.font_family = spec.font_family.empty() ? defaults.font_family : spec.font_family;
    result.font_bold   = spec.font_bold;
    result.font_italic = spec.font_italic;

    result.textDecoration          = spec.textDecoration;
    result.textDecorationStyle     = spec.textDecorationStyle;
    result.textDecorationColor     = spec.textDecorationColor;
    result.textDecorationThickness = spec.textDecorationThickness;
    result.verticalAlignValue      = spec.verticalAlignValue;

    // Borders
    result.borderTop    = spec.borderTop;
    result.borderRight  = spec.borderRight;
    result.borderBottom = spec.borderBottom;
    result.borderLeft   = spec.borderLeft;

    // Background
    result.backgroundColor = spec.backgroundColor;
    result.hasBackground   = spec.hasBackground;

    result.flex = spec.flex;

    // Copy over the flags tracking state
    result.set = spec.set;

    // ============================================================
    // 3. TREE TRAVERSAL
    // ============================================================

    node.computedStyle = result;
    if (node.tag == "button" && !result.set.display) {
        node.computedStyle.display = DisplayType::InlineBlock;
    }
    for (auto& child : node.children)
    {
        ComputeStyle(*child, &node.computedStyle);
    }
}

Node Parser::Parse(const std::vector<Token>& tokens)
{
    Node root;
    root.type = NodeType::Document;

    std::stack<Node*> nodeStack;
    nodeStack.push(&root);

    for (const Token& token : tokens)
    {
        switch (token.type)
        {
            case TokenType::Doctype:
            {
                auto node = std::make_unique<Node>();

                node->type = NodeType::Doctype;
                node->tag = token.value;
                node->parent = nodeStack.top();
                node->attributes = token.attributes;

                ApplyAttributes(*node);

                nodeStack.top()->children.push_back(std::move(node));
                break;
            }

            case TokenType::OpenTag:
            {
                auto node = std::make_unique<Node>();

                node->type = NodeType::Element;
                node->tag = token.value;
                node->parent = nodeStack.top();
                node->attributes = token.attributes;

                ApplyAttributes(*node);

                Node* rawPtr = node.get();

                nodeStack.top()->children.push_back(std::move(node));

                if (!IsVoidElement(token.value))
                    nodeStack.push(rawPtr);

                break;
            }

            case TokenType::Text:
            {
                std::string normalized = NormalizeText(token.value);

                if (normalized.empty() ||
                    IsAllWhitespace(normalized))
                {
                    break;
                }

                auto node = std::make_unique<Node>();

                node->type = NodeType::Text;
                node->text = normalized;
                node->parent = nodeStack.top();

                nodeStack.top()->children.push_back(std::move(node));
                break;
            }

            case TokenType::Comment:
            {
                auto node = std::make_unique<Node>();

                node->type = NodeType::Comment;
                node->text = token.value;
                node->parent = nodeStack.top();

                nodeStack.top()->children.push_back(std::move(node));
                break;
            }

            case TokenType::CloseTag:
            {
                if (nodeStack.size() <= 1)
                    break;

                while (nodeStack.size() > 1)
                {
                    Node* top = nodeStack.top();
                    nodeStack.pop();

                    if (top->tag == token.value)
                        break;
                }

                break;
            }

            default:
                std::cout << "Unknown token type\n";
                break;
        }
    }

    NormalizeDOM(root);

    return root;
}

void Parser::PrintNode(const Node& node, int depth)
{
    for (int i = 0; i < depth; i++)
        std::cout << "  ";

    switch (node.type)
    {
        case NodeType::Doctype:
            std::cout << "DOCTYPE";
            break;

        case NodeType::Document:
            std::cout << "Document";
            break;

        case NodeType::Element:
        {
            std::cout << "<" << node.tag;

            for (const auto& [key, value] : node.attributes)
            {
                std::cout << " "
                          << key
                          << "=\""
                          << value
                          << "\"";
            }

            std::cout << ">";
            break;
        }

        case NodeType::Text:
            std::cout << "TEXT: \"" << node.text << "\"";
            break;

        case NodeType::Comment:
            std::cout << "<!-- " << node.text << " -->";
            break;
    }

    std::cout << '\n';

    for (const auto& child : node.children)
        PrintNode(*child, depth + 1);
}

Node* Parser::FindNodeByTag(Node* dom, const std::string& tag)
{
    if (dom->tag == tag)
        return dom;

    for (auto& child : dom->children)
    {
        if (auto found = FindNodeByTag(child.get(), tag))
            return found;
    }

    return nullptr;
}