//
// Created by tkdtu on 5/27/2026.
//

#ifndef BROWSER_NODE_H
#define BROWSER_NODE_H
#include <string>
#include <memory>
#include <vector>
#include "Color.h"
#include <unordered_map>
enum class OverflowType {
    Visible,
    Hidden,
    Scroll
};

enum class NodeType {
    Document,
    Doctype,
    Element,
    Text,
};

enum class DisplayType {
    Block,
    Inline,
    InlineBlock
};

enum class PositionType {
    Static,
    Relative,
    Absolute
};
enum class TextAlign { Left, Center, Right };
struct Border {
    int top = 0, right = 0, bottom = 0, left = 0;
    Color color = Color(0, 0, 0);

    bool any() const { return top || right || bottom || left; }
};
enum class TextDecoration {
    None,
    Underline,
    Overline,
    LineThrough,
    blink,
    spellingError,
    GrammarError,
};
enum class TextDecorationStyle {
    Solid,
    Double,
    Dotted,
    Dashed,
    Wavy,
};
enum class BoxSizing {
    ContentBox,
    BorderBox
};
struct StyleSetFlags {
    bool font_size : 1 = false;
    bool color : 1 = false;
    bool display : 1 = false;
    bool textAlign : 1 = false;
    bool font_bold : 1 = false;
    bool font_italic : 1 = false;

    bool margin_top : 1 = false;
    bool margin_bottom : 1 = false;
    bool margin_left : 1 = false;
    bool margin_right : 1 = false;

    bool padding_top : 1 = false;
    bool padding_bottom : 1 = false;
    bool padding_left : 1 = false;
    bool padding_right : 1 = false;

    bool width : 1 = false;
    bool height : 1 = false;

    bool background : 1 = false;
};
struct Style {
    DisplayType display = DisplayType::Inline;
    PositionType position = PositionType::Static;

    BoxSizing boxSizing = BoxSizing::ContentBox;

    int margin_top = 0;
    int margin_bottom = 0;
    int margin_left = 0;
    int margin_right = 0;

    int padding_top = 0;
    int padding_bottom = 0;
    int padding_left = 0;
    int padding_right = 0;

    int width = -1;   // -1 = auto
    int height = -1;  // -1 = auto

    Color color;
    int offset_x = 0;
    int offset_y = 0;

    int min_height = -1; // -1 = no minimum
    int max_height = -1; // -1 = no maximum

    int min_width = -1;
    int max_width = -1;

    TextDecoration textDecoration = TextDecoration::None;
    Color TextDecorationColor = Color(0, 0, 0);
    int TextDecorationThickness = 1;
    TextDecorationStyle textDecorationStyle = TextDecorationStyle::Solid;



    std::string font_family;
    int font_size = 0;

    bool font_bold = false;
    bool font_italic = false;



    bool margin_left_auto  = false;
    bool margin_right_auto = false;
    Color backgroundColor = Color(255, 255, 255); // or make Optional if you want transparency
    bool hasBackground = false; // false = don't paint, true = paint backgroundColor
    Border border;

    float font_size_em = 0.0f; // non-zero means "resolve as em during ComputeStyle"
    TextAlign textAlign = TextAlign::Left;

    StyleSetFlags set;
};


struct Node {
    NodeType type;

    std::string tag;
    std::string text;

    Node* parent = nullptr;

    std::vector<std::unique_ptr<Node>> children;
    std::unordered_map<std::string, std::string> attributes;

    Style computedStyle;
    Style specifiedStyle;


    Node() = default;
    ~Node() = default;

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    Node(Node&&) noexcept = default;
    Node& operator=(Node&&) noexcept = default;
};
#endif //BROWSER_NODE_H
