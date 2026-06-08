
#ifndef BROWSER_NODE_H
#define BROWSER_NODE_H

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#include "Color.h"

// ============================================================
// CSS LENGTHS
// ============================================================

enum class LengthUnit {
    Px,
    Percent,
    Auto,
    Em,
    Vw,
    Vh,
    Inherit
};

struct CSSLength {
    float value = 0.0f;
    LengthUnit unit = LengthUnit::Px;
};

// ============================================================
// ENUMS
// ============================================================

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
    Comment,
    Image,
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

enum class TextAlign {
    Left,
    Center,
    Right
};

enum class TextDecoration {
    None,
    Underline,
    Overline,
    LineThrough,
    Blink,
    SpellingError,
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

enum class VerticalAlignKeyword {
    Top,
    Middle,
    Bottom,
    Sub,
    Super,
    TextTop,
    TextBottom,
    Baseline,
    Other,
};

enum class BorderStyle {
    Dotted,
    Dashed,
    Solid,
    DoubleBorder,
    Groove,
    Ridge,
    Inset,
    Outset,
    None,
    Hidden
};

enum class ObjectFit {
    Fill,
    Contain,
    Cover,
    None,
    ScaleDown
};

enum class WhiteSpace {
    Normal,
    NoWrap,
    Pre,
    PreWrap,
    PreLine,
};

enum class TextOverflow {
    Clip,
    Ellipsis,
};

// ============================================================
// IMAGE DATA
// ============================================================

struct ImageData {
    std::vector<Color> pixels;

    int intrinsicWidth = 0;
    int intrinsicHeight = 0;

    bool isLoaded = false;
};

// ============================================================
// BORDER
// ============================================================

struct BorderSide {
    CSSLength borderWidth = { 0.0f, LengthUnit::Px };

    Color borderColor = Color(0, 0, 0);

    BorderStyle borderStyle = BorderStyle::None;
};

// ============================================================
// STYLE FLAGS
// ============================================================

struct StyleSetFlags {
    bool font_size = false;
    bool color = false;
    bool display = false;
    bool textAlign = false;

    bool font_bold = false;
    bool font_italic = false;

    bool margin_top = false;
    bool margin_bottom = false;
    bool margin_left = false;
    bool margin_right = false;

    bool padding_top = false;
    bool padding_bottom = false;
    bool padding_left = false;
    bool padding_right = false;

    bool width = false;
    bool height = false;

    bool min_width = false;
    bool max_width = false;
    bool min_height = false;
    bool max_height = false;

    bool background = false;

    bool textOverflow = false;
    bool whiteSpace = false;
    bool overflow = false;

    bool boxSizing = false;
    bool objectFit = false;

    bool verticalAlign = false;

    bool border_radius_top_left    = false;
    bool border_radius_top_right   = false;
    bool border_radius_bottom_right= false;
    bool border_radius_bottom_left = false;
};

// ============================================================
// SPECIFIED STYLE
// ============================================================

struct Style {
    // --------------------------------------------------------
    // Layout
    // --------------------------------------------------------

    DisplayType display = DisplayType::Inline;

    PositionType position = PositionType::Static;

    BoxSizing boxSizing = BoxSizing::ContentBox;

    ObjectFit objectFit = ObjectFit::Fill;

    OverflowType overflow = OverflowType::Visible;

    // --------------------------------------------------------
    // Borders
    // --------------------------------------------------------

    BorderSide borderTop;
    BorderSide borderRight;
    BorderSide borderBottom;
    BorderSide borderLeft;

    // --------------------------------------------------------
    // Margin
    // --------------------------------------------------------

    CSSLength margin_top    { 0.0f, LengthUnit::Px };
    CSSLength margin_bottom { 0.0f, LengthUnit::Px };
    CSSLength margin_left   { 0.0f, LengthUnit::Px };
    CSSLength margin_right  { 0.0f, LengthUnit::Px };

    // --------------------------------------------------------
    // Padding
    // --------------------------------------------------------

    CSSLength padding_top    { 0.0f, LengthUnit::Px };
    CSSLength padding_bottom { 0.0f, LengthUnit::Px };
    CSSLength padding_left   { 0.0f, LengthUnit::Px };
    CSSLength padding_right  { 0.0f, LengthUnit::Px };

    // --------------------------------------------------------
    // Border Radius
    // --------------------------------------------------------
    CSSLength border_radius_top_left    { 0.0f, LengthUnit::Px };
    CSSLength border_radius_top_right   { 0.0f, LengthUnit::Px };
    CSSLength border_radius_bottom_right{ 0.0f, LengthUnit::Px };
    CSSLength border_radius_bottom_left { 0.0f, LengthUnit::Px };

    // --------------------------------------------------------
    // Dimensions
    // --------------------------------------------------------

    CSSLength width      { 0.0f, LengthUnit::Auto };
    CSSLength height     { 0.0f, LengthUnit::Auto };

    CSSLength min_width  { 0.0f, LengthUnit::Auto };
    CSSLength max_width  { 0.0f, LengthUnit::Auto };

    CSSLength min_height { 0.0f, LengthUnit::Auto };
    CSSLength max_height { 0.0f, LengthUnit::Auto };

    // --------------------------------------------------------
    // Typography
    // --------------------------------------------------------

    std::string font_family;

    CSSLength font_size {16.0f, LengthUnit::Px};

    bool font_bold = false;
    bool font_italic = false;

    Color color = Color(0, 0, 0);

    TextAlign textAlign = TextAlign::Left;

    WhiteSpace whiteSpace = WhiteSpace::Normal;

    TextOverflow textOverflow = TextOverflow::Clip;

    // --------------------------------------------------------
    // Text Decoration
    // --------------------------------------------------------

    TextDecoration textDecoration = TextDecoration::None;

    TextDecorationStyle textDecorationStyle =
        TextDecorationStyle::Solid;

    Color textDecorationColor = Color(0, 0, 0);

    CSSLength textDecorationThickness =
        { 1.0f, LengthUnit::Px };

    // --------------------------------------------------------
    // Background
    // --------------------------------------------------------

    bool hasBackground = false;

    Color backgroundColor = Color(255, 255, 255);

    // --------------------------------------------------------
    // Vertical Align
    // --------------------------------------------------------

    VerticalAlignKeyword verticalAlign =
        VerticalAlignKeyword::Baseline;

    CSSLength verticalAlignValue;

    // --------------------------------------------------------
    // Style state tracking
    // --------------------------------------------------------

    StyleSetFlags set;
};

// ============================================================
// LAYOUT / RENDER
// ============================================================

struct Rect {
    float x = 0.0f;
    float y = 0.0f;

    float width = 0.0f;
    float height = 0.0f;
};

struct RenderData {
    Rect box;

    float resolved_padding_top = 0.0f;
    float resolved_padding_right = 0.0f;
    float resolved_padding_bottom = 0.0f;
    float resolved_padding_left = 0.0f;

    float resolved_margin_top = 0.0f;
    float resolved_margin_right = 0.0f;
    float resolved_margin_bottom = 0.0f;
    float resolved_margin_left = 0.0f;

    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

// ============================================================
// DOM NODE
// ============================================================

struct Node {

    // --------------------------------------------------------
    // DOM Identity
    // --------------------------------------------------------

    NodeType type = NodeType::Element;

    std::string tag;

    std::string id;

    std::string class_name;

    std::vector<std::string> classList;

    // --------------------------------------------------------
    // Content
    // --------------------------------------------------------

    std::string text;

    std::string code;

    std::string script_name;

    // --------------------------------------------------------
    // Tree
    // --------------------------------------------------------

    // Non-owning pointer.
    // Safe because parent owns children through unique_ptr.
    Node* parent = nullptr;

    std::vector<std::unique_ptr<Node>> children;

    // --------------------------------------------------------
    // Attributes
    // --------------------------------------------------------

    std::unordered_map<std::string, std::string> attributes;

    bool HasAttribute(const std::string& key) const
    {
        return attributes.find(key) != attributes.end();
    }

    std::string GetAttribute(
        const std::string& key,
        const std::string& defaultValue = "") const
    {
        auto it = attributes.find(key);

        return (it != attributes.end())
            ? it->second
            : defaultValue;
    }

    // --------------------------------------------------------
    // Styles
    // --------------------------------------------------------

    Style specifiedStyle;

    Style computedStyle;

    // --------------------------------------------------------
    // Render Data
    // --------------------------------------------------------

    RenderData renderData;

    // --------------------------------------------------------
    // Asset payloads
    // --------------------------------------------------------

    std::shared_ptr<ImageData> imageData = nullptr;

    // --------------------------------------------------------
    // Invalidations
    // --------------------------------------------------------

    bool reconstruct = false;

    // --------------------------------------------------------
    // Lifetime
    // --------------------------------------------------------

    Node() = default;

    ~Node() = default;

    Node(const Node&) = delete;

    Node& operator=(const Node&) = delete;

    Node(Node&&) noexcept = default;

    Node& operator=(Node&&) noexcept = default;
};

#endif // BROWSER_NODE_H