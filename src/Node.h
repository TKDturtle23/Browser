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
enum class LengthUnit {
    Px,
    Percent,
    Auto,
    Em,
    Inherit
};

struct CSSLength {
    float value = 0.0f;
    LengthUnit unit = LengthUnit::Px;

    // Helper to check if a rule is explicitly provided
    bool IsSpecified() const { return unit != LengthUnit::Auto; }

    // Helper to resolve the value if it's explicitly pixels
    int GetFixedPx_Or(int fallback) const {
        return (unit == LengthUnit::Px) ? static_cast<int>(value) : fallback;
    }
};
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
    Image,
};
struct ImageData {
    std::vector<Color> pixels; // Flat array outputted from your custom decoder
    int intrinsicWidth = 0;    // Real image width from file header
    int intrinsicHeight = 0;   // Real image height from file header
    bool isLoaded = false;
};
enum class DisplayType {
    Block,
    Inline,
    InlineBlock,
    None,
};

enum class PositionType {
    Static,
    Relative,
    Absolute
};
enum class TextAlign { Left, Center, Right };
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

enum class VerticalAlign {
    Top,
    Middle,
    Bottom,
    Sub,
    Super,
    TextTop,
    TextBottom,
    Baseline,
    Inherit,
    Initial,
    Other,
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

    bool textOverflow : 1 = false;
    bool whiteSpace : 1 = false;
    bool overflow : 1 = false;

    bool boxSizing : 1 = false;
    bool objectFit : 1 = false;

    bool min_width : 1 = false;
    bool max_width : 1 = false;
    bool min_height : 1 = false;
    bool max_height : 1 = false;

    bool verticalAlign : 1 = false;
};
enum class BorderStyle {
    dotted,
    dashed,
    solid,
    double_border,
    groove,
    ridge,
    inset,
    outset,
    none,
    hidden
};
enum class ObjectFit {
    Fill,
    Contain,
    Cover,
    None,
    Scale_Down
};
enum class WhiteSpace {
    normal,
    nowrap,
    pre,
    pre_wrap,
    pre_line,
};
enum class TextOverflow {
    Clip,
    Ellipsis,
};

struct Border_side {
    CSSLength borderWidth = { 0.0f, LengthUnit::Px };
    Color borderColor = Color(0, 0, 0);
    BorderStyle borderStyle = BorderStyle::none;
};
struct Style {
    DisplayType display = DisplayType::Inline;
    PositionType position = PositionType::Static;

    BoxSizing boxSizing = BoxSizing::ContentBox;

    Border_side BorderTop = Border_side();
    Border_side BorderRight = Border_side();
    Border_side BorderBottom = Border_side();
    Border_side BorderLeft = Border_side();


    ObjectFit objectFit = ObjectFit::Fill;
    // Margins
    CSSLength margin_top    { 0.0f, LengthUnit::Px };
    CSSLength margin_bottom { 0.0f, LengthUnit::Px };
    CSSLength margin_left   { 0.0f, LengthUnit::Px };
    CSSLength margin_right  { 0.0f, LengthUnit::Px };

    // Paddings
    CSSLength padding_top   { 0.0f, LengthUnit::Px };
    CSSLength padding_bottom{ 0.0f, LengthUnit::Px };
    CSSLength padding_left  { 0.0f, LengthUnit::Px };
    CSSLength padding_right { 0.0f, LengthUnit::Px };

    // Structural Dimensions
    CSSLength width         { 0.0f, LengthUnit::Auto };
    CSSLength height        { 0.0f, LengthUnit::Auto };
    CSSLength min_width     { 0.0f, LengthUnit::Auto };
    CSSLength max_width     { 0.0f, LengthUnit::Auto };
    CSSLength min_height    { 0.0f, LengthUnit::Auto };
    CSSLength max_height    { 0.0f, LengthUnit::Auto };

    Color color;
    int offset_x = 0;
    int offset_y = 0;


    TextDecoration textDecoration = TextDecoration::None;
    Color TextDecorationColor = Color(0, 0, 0);
    CSSLength TextDecorationThickness = { 1.0f, LengthUnit::Px };
    TextDecorationStyle textDecorationStyle = TextDecorationStyle::Solid;

    WhiteSpace whiteSpace = WhiteSpace::normal;
    TextOverflow textOverflow = TextOverflow::Clip;
    OverflowType overflow = OverflowType::Visible;

    std::string font_family;
    CSSLength font_size = {100.0f, LengthUnit::Percent};

    bool font_bold = false;
    bool font_italic = false;
    Color backgroundColor = Color(255, 255, 255); // or make Optional if you want transparency
    bool hasBackground = false; // false = don't paint, true = paint backgroundColor


    TextAlign textAlign = TextAlign::Left;

    VerticalAlign verticalAlign = VerticalAlign::Baseline;
    CSSLength verticalAlignValue; // used if verticalAlign::Other

    StyleSetFlags set;

};


struct Node {
    NodeType type;

    std::string tag;

    std::string text;

    Node* parent = nullptr;

    std::vector<std::unique_ptr<Node>> children;
    std::unordered_map<std::string, std::string> attributes;
    // Quick helper to check if an attribute exists
    bool HasAttribute(const std::string& key) const {
        return attributes.find(key) != attributes.end();
    }

    // Quick helper to get a value safely
    std::string GetAttribute(const std::string& key, const std::string& defaultValue = "") const {
        auto it = attributes.find(key);
        return (it != attributes.end()) ? it->second : defaultValue;
    }
    Style computedStyle;
    Style specifiedStyle;


    // Attached asset payload if type == NodeType::Image
    std::shared_ptr<ImageData> imageData = nullptr;

    Node() = default;
    ~Node() = default;

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    Node(Node&&) noexcept = default;
    Node& operator=(Node&&) noexcept = default;
};
#endif //BROWSER_NODE_H
