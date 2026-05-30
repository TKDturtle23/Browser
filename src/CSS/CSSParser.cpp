#include "CSSParser.h"
#include "CSSTokenizer.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <optional>
#include <sstream>

// ── Selector parsing ────────────────────────────────────────────────────────

// Change parameter to std::string_view
CSSSelector CSSParser::ParseSelector(std::string_view s) {
    CSSSelector sel;

    // strip pseudo-class without copying
    size_t colon = s.find(':');
    if (colon != std::string_view::npos)
        s = s.substr(0, colon);

    // trim without copying
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) return sel;
    size_t end = s.find_last_not_of(" \t\n\r");
    s = s.substr(start, end - start + 1);

    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '#') {
            i++;
            size_t j = i;
            while (j < s.size() && s[j] != '.' && s[j] != '#') j++;
            sel.id = s.substr(i, j - i); // If sel.id must be std::string, it copies here, which is fine.
            i = j;
        } else if (s[i] == '.') {
            i++;
            size_t j = i;
            while (j < s.size() && s[j] != '.' && s[j] != '#') j++;
            sel.cls = s.substr(i, j - i);
            i = j;
        } else {
            size_t j = i;
            while (j < s.size() && s[j] != '.' && s[j] != '#') j++;
            sel.tag = s.substr(i, j - i);
            i = j;
        }
    }
    return sel;
}
// High-performance string_view splitter that doesn't allocate memory
void SplitShorthand(std::string_view v, std::vector<std::string_view>& parts) {
    size_t start = v.find_first_not_of(" \t");
    while (start != std::string_view::npos) {
        size_t end = v.find_first_of(" \t", start);
        if (end == std::string_view::npos) {
            parts.push_back(v.substr(start));
            break;
        }
        parts.push_back(v.substr(start, end - start));
        start = v.find_first_not_of(" \t", end);
    }
}
// ── Rule parsing ─────────────────────────────────────────────────────────────

std::vector<CSSRule> CSSParser::Parse(const std::string& css, bool isInlineStyle) {
    CSSTokenizer tokenizer;
    auto tokens = tokenizer.Tokenize(css, isInlineStyle);

    std::vector<CSSRule> rules;
    CSSRule current;

    // FIX 1: Initialize block tracking context directly from the configuration
    bool inBlock = isInlineStyle;

    // If it's an inline style, inject a universal wild-card selector so it matches
    // whatever element it belongs to automatically
    if (isInlineStyle) {
        CSSSelector universalSelector; // empty tag/id/class matches contextually
        current.selectors.push_back(universalSelector);
    }

    for (size_t i = 0; i < tokens.size(); i++) {
        const CSSToken& tok = tokens[i];

        switch (tok.type) {
            case CSSTokenType::Selector:
                current.selectors.push_back(ParseSelector(tok.value));
                break;

            case CSSTokenType::Comma:
                break;

            case CSSTokenType::OpenBrace:
                inBlock = true;
                break;

            case CSSTokenType::CloseBrace:
                if (!current.selectors.empty() && !current.declarations.empty())
                    rules.push_back(current);
                current = {};
                inBlock = false;
                break;

            case CSSTokenType::Property:
                if (inBlock && i + 1 < tokens.size()
                    && tokens[i + 1].type == CSSTokenType::Value)
                {
                    current.declarations.push_back({ tok.value, tokens[i + 1].value });
                    i++; // consume value token
                }
                break;

            default:
                break;
        }
    }

    // FIX 2: If we are handling an inline style string block, there's no closing brace '}'.
    // We flush out whatever declarations were gathered directly into our rule list.
    if (isInlineStyle && !current.declarations.empty()) {
        rules.push_back(current);
    }

    return rules;
}

// ── Matching ─────────────────────────────────────────────────────────────────

bool CSSParser::Matches(const CSSSelector& sel, const Node& node) {
    if (node.type != NodeType::Element) return false;

    if (!sel.tag.empty() && sel.tag != node.tag) return false;

    if (!sel.id.empty()) {
        auto it = node.attributes.find("id");
        if (it == node.attributes.end() || it->second != sel.id) return false;
    }

    if (!sel.cls.empty()) {
        auto it = node.attributes.find("class");
        if (it == node.attributes.end()) return false;

        std::string_view class_attr = it->second;
        size_t start = class_attr.find_first_not_of(" \t");
        bool found = false;

        while (start != std::string_view::npos) {
            size_t end = class_attr.find_first_of(" \t", start);
            std::string_view current_class = (end == std::string_view::npos)
                ? class_attr.substr(start)
                : class_attr.substr(start, end - start);

            if (current_class == sel.cls) {
                found = true;
                break;
            }

            if (end == std::string_view::npos) break;
            start = class_attr.find_first_not_of(" \t", end);
        }

        if (!found) return false;
    }

    return true;
}




static std::string Trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    return (start < end) ? std::string(start, end) : "";
}
static std::optional<CSSLength> ParseCSSLength(const std::string& val, int vw, int vh) {
    if (val.empty()) return std::nullopt;
    if (val == "auto") return CSSLength{ 0.0f, LengthUnit::Auto };

    try {
        size_t len = val.size();

        // 1. Check for single-character unit (%) first to avoid substring length confusion
        if (len > 0 && val.back() == '%') {
            std::string num_part = val.substr(0, len - 1);
            if (num_part.empty()) return std::nullopt; // Handles just "%"

            float pct = std::stof(num_part);
            return CSSLength{ pct, LengthUnit::Percent };
        }
        // 2. Check for 2-character units
        if (len > 2) {
            std::string unit = val.substr(len - 2);
            if (unit == "vw") {
                float px = (std::stof(val.substr(0, len - 2)) / 100.0f) * vw;
                return CSSLength{ px, LengthUnit::Px };
            }
            if (unit == "vh") {
                float px = (std::stof(val.substr(0, len - 2)) / 100.0f) * vh;
                return CSSLength{ px, LengthUnit::Px };
            }
            if (unit == "px") {
                return CSSLength{ std::stof(val.substr(0, len - 2)), LengthUnit::Px };
            }
            if (unit == "em") {
                return CSSLength{ std::stof(val.substr(0, len - 2)), LengthUnit::Em };
            }
        }

        // Fallback for raw numbers without unit strings
        return CSSLength{ std::stof(val), LengthUnit::Px };
    } catch (...) {
        return std::nullopt; // Safe fallback on corrupted/unparsed input
    }
}
static std::optional<Color> ParseColor(const std::string& val) {
    if (val.empty()) return std::nullopt;

    // #rgb shorthand
    if (val[0] == '#' && val.size() == 4) {
        auto expand = [](char c) -> uint8_t {
            int v = std::isdigit(c) ? c - '0' : std::tolower(c) - 'a' + 10;
            return static_cast<uint8_t>(v * 17);
        };
        return Color(expand(val[3]), expand(val[2]), expand(val[1]));
    }

    // #rrggbb
    if (val[0] == '#' && val.size() == 7) {
        auto hex2 = [](char high, char low) -> uint8_t {
            auto h = (high >= 'a') ? (high - 'a' + 10) : ((high >= 'A') ? (high - 'A' + 10) : (high - '0'));
            auto l = (low >= 'a') ? (low - 'a' + 10) : ((low >= 'A') ? (low - 'A' + 10) : (low - '0'));
            return (h << 4) | l;
        };
        // Expecting #RRGGBB format
        return Color(hex2(val[1], val[2]), hex2(val[3], val[4]), hex2(val[5], val[6]));
    }

    // Named colors
    if (val == "white") return Color(255, 255, 255); // same either way
    if (val == "black") return Color(0,   0,   0);   // same either way
    if (val == "red")   return Color(0,   0,   255); // B=0, G=0, R=255 → stored as BGR
    if (val == "green") return Color(0,   128, 0);   // same
    if (val == "blue")  return Color(255, 0,   0);   // B=255 first
    if (val == "gray" || val == "grey") return Color(128, 128, 128); // same
    if (val == "yellow") return Color(0, 255, 255);
    if (val == "orange") return Color(0, 165, 255);
    if (val == "cyan") return Color(0, 255, 255);
    if (val == "magenta") return Color(255, 0, 255);
    if (val == "purple") return Color(128, 0, 128);
    if (val == "brown") return Color(139, 69, 19);
    if (val == "pink") return Color(255, 192, 203);
    if (val == "lime") return Color(0, 255, 0);
    if (val == "teal") return Color(0, 128, 128);
    if (val == "navy") return Color(0, 0, 128);
    if (val == "olive") return Color(128, 128, 0);
    if (val == "silver") return Color(192, 192, 192);
    if (val == "aqua") return Color(0, 255, 255);
    if (val == "fuchsia") return Color(255, 0, 255);
    if (val == "maroon") return Color(128, 0, 0);

    return std::nullopt; // Explicit failure instead of falling back to black
}

static std::optional<BorderStyle> ParseBorderStyle(const std::string& val) {
    if (val == "dotted") return BorderStyle::dotted;
    if (val == "dashed") return BorderStyle::dashed;
    if (val == "solid")  return BorderStyle::solid;
    if (val == "double") return BorderStyle::double_border;
    if (val == "groove") return BorderStyle::groove;
    if (val == "ridge")  return BorderStyle::ridge;
    if (val == "inset")  return BorderStyle::inset;
    if (val == "outset") return BorderStyle::outset;
    if (val == "none")   return BorderStyle::none;
    if (val == "hidden") return BorderStyle::hidden;

    return std::nullopt; // Not a valid border style token
}
//#define CSS_DEBUG
#ifdef CSS_DEBUG
  #define CSS_WARN(msg) do { std::cerr << "[CSS] " << msg << "\n"; } while(0)
#else
  #define CSS_WARN(msg) ((void)0)
#endif

void CSSParser::ApplyDeclarations(const std::vector<CSSDeclaration> &decls,
                                  Node &node,
                                  int vw,
                                  int vh)
{
    for (const auto& d : decls)
    {
        const std::string& p = d.property;
        const std::string& v = d.value;

        // ── color / background ───────────────────────────────────────────────

        if (p == "background" || p == "background-color")
        {
            auto c = ParseColor(v);
            if (!c) CSS_WARN("Failed to parse background color: \"" << v << "\"");
            node.specifiedStyle.set.background = true;
            node.specifiedStyle.hasBackground = true;
            node.specifiedStyle.backgroundColor = c.value_or(Color(0, 0, 0));
        }

        // ── border shorthand ─────────────────────────────────────────────────
        else if (p == "border") {
            std::vector<std::string> parts;
            std::istringstream ss(v);
            std::string part;
            while (ss >> part) parts.push_back(part);

            for (auto& c : parts) {
                auto widthVal = ParseCSSLength(c, vw, vh);
                if (widthVal.has_value()) {
                    node.specifiedStyle.BorderTop.borderWidth    = widthVal.value();
                    node.specifiedStyle.BorderBottom.borderWidth = widthVal.value();
                    node.specifiedStyle.BorderLeft.borderWidth   = widthVal.value();
                    node.specifiedStyle.BorderRight.borderWidth  = widthVal.value();
                    continue;
                }

                if (auto style = ParseBorderStyle(c)) {
                    node.specifiedStyle.BorderTop.borderStyle    = *style;
                    node.specifiedStyle.BorderBottom.borderStyle = *style;
                    node.specifiedStyle.BorderLeft.borderStyle   = *style;
                    node.specifiedStyle.BorderRight.borderStyle  = *style;
                    continue;
                }

                if (auto color = ParseColor(c)) {
                    node.specifiedStyle.BorderTop.borderColor    = *color;
                    node.specifiedStyle.BorderBottom.borderColor = *color;
                    node.specifiedStyle.BorderLeft.borderColor   = *color;
                    node.specifiedStyle.BorderRight.borderColor  = *color;
                    continue;
                }

                CSS_WARN("Unrecognized token in border shorthand: \"" << c << "\"");
            }
        }

        // ── border direction longhands ────────────────────────────────────────
        else if (p == "border-top" || p == "border-bottom" || p == "border-left" || p == "border-right") {
            std::vector<std::string> parts;
            std::istringstream ss(v);
            std::string part;
            while (ss >> part) parts.push_back(part);

            auto* b = &node.specifiedStyle.BorderTop;
            if (p == "border-bottom") b = &node.specifiedStyle.BorderBottom;
            if (p == "border-left")   b = &node.specifiedStyle.BorderLeft;
            if (p == "border-right")  b = &node.specifiedStyle.BorderRight;

            for (auto& c : parts) {
                auto w = ParseCSSLength(c, vw, vh);
                if (w.has_value()) { b->borderWidth = w.value(); continue; }

                if (auto style = ParseBorderStyle(c)) { b->borderStyle = *style; continue; }
                if (auto color = ParseColor(c))       { b->borderColor = *color; continue; }

                CSS_WARN("Unrecognized token in " << p << " shorthand: \"" << c << "\"");
            }
        }

        // ── border component longhands ────────────────────────────────────────
        else if (p == "border-width") {
            auto w = ParseCSSLength(v, vw, vh);
            if (w.has_value()) {
                node.specifiedStyle.BorderTop.borderWidth    = w.value();
                node.specifiedStyle.BorderBottom.borderWidth = w.value();
                node.specifiedStyle.BorderLeft.borderWidth   = w.value();
                node.specifiedStyle.BorderRight.borderWidth  = w.value();
            } else {
                CSS_WARN("Failed to parse border-width value: \"" << v << "\"");
            }
        }
        else if (p == "border-style") {
            auto s = ParseBorderStyle(v);
            if (!s) CSS_WARN("Failed to parse border-style value: \"" << v << "\"");
            BorderStyle bs = s.value_or(BorderStyle::none);
            node.specifiedStyle.BorderTop.borderStyle    = bs;
            node.specifiedStyle.BorderBottom.borderStyle = bs;
            node.specifiedStyle.BorderLeft.borderStyle   = bs;
            node.specifiedStyle.BorderRight.borderStyle  = bs;
        }
        else if (p == "border-color") {
            auto c = ParseColor(v);
            if (!c) CSS_WARN("Failed to parse border-color value: \"" << v << "\"");
            Color bc = c.value_or(Color(0,0,0));
            node.specifiedStyle.BorderTop.borderColor    = bc;
            node.specifiedStyle.BorderBottom.borderColor = bc;
            node.specifiedStyle.BorderLeft.borderColor   = bc;
            node.specifiedStyle.BorderRight.borderColor  = bc;
        }
        else if (p == "color")
        {
            auto c = ParseColor(v);
            if (!c) CSS_WARN("Failed to parse color value: \"" << v << "\"");
            node.specifiedStyle.set.color = true;
            node.specifiedStyle.color = c.value_or(Color(0,0,0));
        }

        // ── font ─────────────────────────────────────────────────────────────

        else if (p == "font-size")
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) {
                node.specifiedStyle.set.font_size = true;
                node.specifiedStyle.font_size = *val;
            } else {
                CSS_WARN("Failed to parse font-size value: \"" << v << "\"");
            }
        }
        else if (p == "font-weight")
        {
            node.specifiedStyle.set.font_bold = true;
            if (v == "bold" || v == "700" || v == "800" || v == "900")
                node.specifiedStyle.font_bold = true;
            else if (v == "normal" || v == "400")
                node.specifiedStyle.font_bold = false;
            else {
                CSS_WARN("Unrecognized font-weight value: \"" << v << "\" (defaulting to not bold)");
                node.specifiedStyle.font_bold = false;
            }
        }
        else if (p == "font-style")
        {
            node.specifiedStyle.set.font_italic = true;
            if (v == "italic" || v == "oblique")
                node.specifiedStyle.font_italic = true;
            else if (v == "normal")
                node.specifiedStyle.font_italic = false;
            else {
                CSS_WARN("Unrecognized font-style value: \"" << v << "\" (defaulting to normal)");
                node.specifiedStyle.font_italic = false;
            }
        }

        // ── margin shorthand ────────────────────────────────────────────────

        else if (p == "margin")
        {
            std::vector<std::string> parts;
            std::istringstream ss(v);
            std::string part;
            while (ss >> part) parts.push_back(part);

            CSSLength defaultLen = {0, LengthUnit::Px};
            CSSLength top = defaultLen, right = defaultLen, bottom = defaultLen, left = defaultLen;

            if (parts.size() == 1)
            {
                auto val = ParseCSSLength(parts[0], vw, vh);
                if (val) top = right = bottom = left = *val;
                else CSS_WARN("Failed to parse margin value: \"" << parts[0] << "\"");
            }
            else if (parts.size() == 2)
            {
                auto v_val = ParseCSSLength(parts[0], vw, vh);
                auto h_val = ParseCSSLength(parts[1], vw, vh);
                if (v_val) top = bottom = *v_val;
                else CSS_WARN("Failed to parse margin vertical value: \"" << parts[0] << "\"");
                if (h_val) right = left = *h_val;
                else CSS_WARN("Failed to parse margin horizontal value: \"" << parts[1] << "\"");
            }
            else if (parts.size() == 3)
            {
                auto t_val = ParseCSSLength(parts[0], vw, vh);
                auto h_val = ParseCSSLength(parts[1], vw, vh);
                auto b_val = ParseCSSLength(parts[2], vw, vh);
                if (t_val) top = *t_val;
                else CSS_WARN("Failed to parse margin-top value: \"" << parts[0] << "\"");
                if (h_val) right = left = *h_val;
                else CSS_WARN("Failed to parse margin horizontal value: \"" << parts[1] << "\"");
                if (b_val) bottom = *b_val;
                else CSS_WARN("Failed to parse margin-bottom value: \"" << parts[2] << "\"");
            }
            else if (parts.size() == 4)
            {
                auto t_val = ParseCSSLength(parts[0], vw, vh);
                auto r_val = ParseCSSLength(parts[1], vw, vh);
                auto b_val = ParseCSSLength(parts[2], vw, vh);
                auto l_val = ParseCSSLength(parts[3], vw, vh);
                if (t_val) top    = *t_val; else CSS_WARN("Failed to parse margin-top value: \""    << parts[0] << "\"");
                if (r_val) right  = *r_val; else CSS_WARN("Failed to parse margin-right value: \""  << parts[1] << "\"");
                if (b_val) bottom = *b_val; else CSS_WARN("Failed to parse margin-bottom value: \"" << parts[2] << "\"");
                if (l_val) left   = *l_val; else CSS_WARN("Failed to parse margin-left value: \""   << parts[3] << "\"");
            }
            else {
                CSS_WARN("margin shorthand has unexpected number of parts (" << parts.size() << "): \"" << v << "\"");
            }

            node.specifiedStyle.set.margin_top    = true;
            node.specifiedStyle.set.margin_bottom = true;
            node.specifiedStyle.set.margin_left   = true;
            node.specifiedStyle.set.margin_right  = true;

            node.specifiedStyle.margin_top    = top;
            node.specifiedStyle.margin_bottom = bottom;
            node.specifiedStyle.margin_left   = left;
            node.specifiedStyle.margin_right  = right;
        }

        // ── margin longhands ────────────────────────────────────────────────

        else if (p == "margin-top")
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.margin_top = true; node.specifiedStyle.margin_top = *val; }
            else CSS_WARN("Failed to parse margin-top value: \"" << v << "\"");
        }
        else if (p == "margin-bottom")
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.margin_bottom = true; node.specifiedStyle.margin_bottom = *val; }
            else CSS_WARN("Failed to parse margin-bottom value: \"" << v << "\"");
        }
        else if (p == "margin-left")
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.margin_left = true; node.specifiedStyle.margin_left = *val; }
            else CSS_WARN("Failed to parse margin-left value: \"" << v << "\"");
        }
        else if (p == "margin-right")
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.margin_right = true; node.specifiedStyle.margin_right = *val; }
            else CSS_WARN("Failed to parse margin-right value: \"" << v << "\"");
        }

        // ── padding shorthand ─────────────────────────────────────────────────

        else if (p == "padding")
        {
            std::vector<std::string> parts;
            std::istringstream ss(v);
            std::string part;
            while (ss >> part) parts.push_back(part);

            CSSLength defaultLen = {0, LengthUnit::Px};
            CSSLength top = defaultLen, right = defaultLen, bottom = defaultLen, left = defaultLen;

            if (parts.empty()) {
                CSS_WARN("padding shorthand is empty");
            }
            else if (parts.size() == 1) {
                auto val = ParseCSSLength(parts[0], vw, vh);
                if (val) top = right = bottom = left = *val;
                else CSS_WARN("Failed to parse padding value: \"" << parts[0] << "\"");
            }
            else if (parts.size() == 2) {
                auto v_val = ParseCSSLength(parts[0], vw, vh);
                auto h_val = ParseCSSLength(parts[1], vw, vh);
                if (v_val) top = bottom = *v_val;
                else CSS_WARN("Failed to parse padding vertical value: \"" << parts[0] << "\"");
                if (h_val) right = left = *h_val;
                else CSS_WARN("Failed to parse padding horizontal value: \"" << parts[1] << "\"");
            }
            else if (parts.size() >= 3) {
                auto t_val = ParseCSSLength(parts[0], vw, vh);
                auto r_val = ParseCSSLength(parts[1], vw, vh);
                auto b_val = ParseCSSLength(parts[2], vw, vh);
                if (t_val) top   = *t_val; else CSS_WARN("Failed to parse padding-top value: \""   << parts[0] << "\"");
                if (r_val) right = left = *r_val; else CSS_WARN("Failed to parse padding-right value: \"" << parts[1] << "\"");
                if (b_val) bottom = *b_val; else CSS_WARN("Failed to parse padding-bottom value: \"" << parts[2] << "\"");

                if (parts.size() == 4) {
                    auto l_val = ParseCSSLength(parts[3], vw, vh);
                    if (l_val) left = *l_val;
                    else CSS_WARN("Failed to parse padding-left value: \"" << parts[3] << "\"");
                } else if (parts.size() > 4) {
                    CSS_WARN("padding shorthand has too many parts (" << parts.size() << "): \"" << v << "\"");
                }
            }

            node.specifiedStyle.set.padding_top    = true;
            node.specifiedStyle.set.padding_bottom = true;
            node.specifiedStyle.set.padding_left   = true;
            node.specifiedStyle.set.padding_right  = true;

            node.specifiedStyle.padding_top    = top;
            node.specifiedStyle.padding_bottom = bottom;
            node.specifiedStyle.padding_left   = left;
            node.specifiedStyle.padding_right  = right;
        }

        // ── padding longhands ───────────────────────────────────────────────

        else if (p == "padding-top")
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.padding_top = true; node.specifiedStyle.padding_top = *val; }
            else CSS_WARN("Failed to parse padding-top value: \"" << v << "\"");
        }
        else if (p == "padding-bottom")
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.padding_bottom = true; node.specifiedStyle.padding_bottom = *val; }
            else CSS_WARN("Failed to parse padding-bottom value: \"" << v << "\"");
        }
        else if (p == "padding-left")
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.padding_left = true; node.specifiedStyle.padding_left = *val; }
            else CSS_WARN("Failed to parse padding-left value: \"" << v << "\"");
        }
        else if (p == "padding-right")
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.padding_right = true; node.specifiedStyle.padding_right = *val; }
            else CSS_WARN("Failed to parse padding-right value: \"" << v << "\"");
        }

        // ── size ────────────────────────────────────────────────────────────

        else if (p == "width")
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.width = true; node.specifiedStyle.width = *parsed; }
            else CSS_WARN("Failed to parse width value: \"" << v << "\"");
        }
        else if (p == "min-width")
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.min_width = true; node.specifiedStyle.min_width = *parsed; }
            else CSS_WARN("Failed to parse min-width value: \"" << v << "\"");
        }
        else if (p == "max-width")
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.max_width = true; node.specifiedStyle.max_width = *parsed; }
            else CSS_WARN("Failed to parse max-width value: \"" << v << "\"");
        }
        else if (p == "height")
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.height = true; node.specifiedStyle.height = *parsed; }
            else CSS_WARN("Failed to parse height value: \"" << v << "\"");
        }
        else if (p == "min-height")
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.min_height = true; node.specifiedStyle.min_height = *parsed; }
            else CSS_WARN("Failed to parse min-height value: \"" << v << "\"");
        }
        else if (p == "max-height")
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.max_height = true; node.specifiedStyle.max_height = *parsed; }
            else CSS_WARN("Failed to parse max-height value: \"" << v << "\"");
        }

        // ── text alignment ────────────────────────────────────────────────

        else if (p == "text-align")
        {
            node.specifiedStyle.set.textAlign = true;
            if (v == "left")
                node.specifiedStyle.textAlign = TextAlign::Left;
            else if (v == "center")
                node.specifiedStyle.textAlign = TextAlign::Center;
            else if (v == "right")
                node.specifiedStyle.textAlign = TextAlign::Right;
            else {
                CSS_WARN("Unrecognized text-align value: \"" << v << "\" (defaulting to left)");
                node.specifiedStyle.textAlign = TextAlign::Left;
            }
        }

        // ── text decoration ───────────────────────────────────────────────

        else if (p == "text-decoration")
        {
            std::stringstream ss(v);
            std::string word;
            std::vector<std::string> words;
            while (ss >> word) words.push_back(word);

            for (const auto& w : words)
            {
                if (w == "underline")
                    node.specifiedStyle.textDecoration = TextDecoration::Underline;
                else if (w == "line-through")
                    node.specifiedStyle.textDecoration = TextDecoration::LineThrough;
                else if (w == "overline")
                    node.specifiedStyle.textDecoration = TextDecoration::Overline;
                else if (w == "none")
                    node.specifiedStyle.textDecoration = TextDecoration::None;
                else if (w == "blink")
                    node.specifiedStyle.textDecoration = TextDecoration::blink;
                else if (w == "spelling-error")
                    node.specifiedStyle.textDecoration = TextDecoration::spellingError;
                else if (w == "grammar-error")
                    node.specifiedStyle.textDecoration = TextDecoration::GrammarError;
                else if (w == "wavy")
                    node.specifiedStyle.textDecorationStyle = TextDecorationStyle::Wavy;
                else if (w == "dotted")
                    node.specifiedStyle.textDecorationStyle = TextDecorationStyle::Dotted;
                else if (w == "dashed")
                    node.specifiedStyle.textDecorationStyle = TextDecorationStyle::Dashed;
                else if (w == "solid")
                    node.specifiedStyle.textDecorationStyle = TextDecorationStyle::Solid;
                else if (w == "double")
                    node.specifiedStyle.textDecorationStyle = TextDecorationStyle::Double;
                else if (auto lenVal = ParseCSSLength(w, vw, vh))
                    node.specifiedStyle.TextDecorationThickness = *lenVal;
                else if (auto colVal = ParseColor(w))
                    node.specifiedStyle.TextDecorationColor = *colVal;
                else
                    CSS_WARN("Unrecognized token in text-decoration: \"" << w << "\"");
            }
        }

        // ── layout ──────────────────────────────────────────────────────────

        else if (p == "display")
        {
            node.specifiedStyle.set.display = true;
            if (v == "block")
                node.specifiedStyle.display = DisplayType::Block;
            else if (v == "inline" || v == "inline-block")
                node.specifiedStyle.display = DisplayType::Inline;
            else if (v == "none") {
                node.specifiedStyle.display = DisplayType::None;
                node.specifiedStyle.set.width = true;
                node.specifiedStyle.set.height = true;
                node.specifiedStyle.width = CSSLength{0, LengthUnit::Px};
                node.specifiedStyle.height = CSSLength{0, LengthUnit::Px};
            }
            else {
                CSS_WARN("Unrecognized display value: \"" << v << "\" (defaulting to inline)");
                node.specifiedStyle.display = DisplayType::Inline;
            }
        }
        else if (p == "box-sizing")
        {
            node.specifiedStyle.set.boxSizing = true;
            if (v == "border-box")
                node.specifiedStyle.boxSizing = BoxSizing::BorderBox;
            else if (v == "content-box")
                node.specifiedStyle.boxSizing = BoxSizing::ContentBox;
            else
                CSS_WARN("Unrecognized box-sizing value: \"" << v << "\"");
        }
        else if (p == "white-space")
        {
            node.specifiedStyle.set.whiteSpace = true;
            if (v == "nowrap")
                node.specifiedStyle.whiteSpace = WhiteSpace::nowrap;
            else if (v == "normal")
                node.specifiedStyle.whiteSpace = WhiteSpace::normal;
            else {
                CSS_WARN("Unrecognized white-space value: \"" << v << "\" (defaulting to normal)");
                node.specifiedStyle.whiteSpace = WhiteSpace::normal;
            }
        }
        else if (p == "text-overflow")
        {
            node.specifiedStyle.set.textOverflow = true;
            if (v == "ellipsis")
                node.specifiedStyle.textOverflow = TextOverflow::Ellipsis;
            else if (v == "clip")
                node.specifiedStyle.textOverflow = TextOverflow::Clip;
            else {
                CSS_WARN("Unrecognized text-overflow value: \"" << v << "\" (defaulting to clip)");
                node.specifiedStyle.textOverflow = TextOverflow::Clip;
            }
        }

        // ── images ───────────────────────────────────────────────────────────

        else if (p == "object-fit")
        {
            node.specifiedStyle.set.objectFit = true;
            if (v == "contain")
                node.specifiedStyle.objectFit = ObjectFit::Contain;
            else if (v == "cover")
                node.specifiedStyle.objectFit = ObjectFit::Cover;
            else if (v == "fill")
                node.specifiedStyle.objectFit = ObjectFit::Fill;
            else if (v == "none")
                node.specifiedStyle.objectFit = ObjectFit::None;
            else {
                CSS_WARN("Unrecognized object-fit value: \"" << v << "\" (defaulting to none)");
                node.specifiedStyle.objectFit = ObjectFit::None;
            }
        }
        else if (p == "vertical-align") {
            node.specifiedStyle.set.verticalAlign = true;
            if (v == "baseline")
                node.specifiedStyle.verticalAlign = VerticalAlign::Baseline;
            else if (v == "sub")
                node.specifiedStyle.verticalAlign = VerticalAlign::Sub;
            else if (v == "super")
                node.specifiedStyle.verticalAlign = VerticalAlign::Super;
            else if (v == "top")
                node.specifiedStyle.verticalAlign = VerticalAlign::Top;
            else if (v == "text-top")
                node.specifiedStyle.verticalAlign = VerticalAlign::TextTop;
            else if (v == "middle")
                node.specifiedStyle.verticalAlign = VerticalAlign::Middle;
            else if (v == "bottom")
                node.specifiedStyle.verticalAlign = VerticalAlign::Bottom;
            else if (v == "text-bottom")
                node.specifiedStyle.verticalAlign = VerticalAlign::TextBottom;
            else if (ParseCSSLength(v, vw, vh).has_value()) {
                node.specifiedStyle.verticalAlign = VerticalAlign::Other;
                node.specifiedStyle.verticalAlignValue = ParseCSSLength(v, vw, vh).value();
            }

        }
        // ── unrecognized ─────────────────────────────────────────────────────

        else
        {
            CSS_WARN("Unrecognized CSS property: \"" << p << "\": \"" << v << "\"");
        }
    }
}
// ── Tree traversal ───────────────────────────────────────────────────────────

// CSSParser.cpp
void CSSParser::ApplyToTree(const std::vector<CSSRule>& globalRules, Node& node, int vw, int vh) {
    if (node.type == NodeType::Element) {
        // 1. Process Global Stylesheets (Cascading Rules)
        for (const auto& rule : globalRules) {
            for (const auto& sel : rule.selectors) {
                if (Matches(sel, node)) {
                    ApplyDeclarations(rule.declarations, node, vw, vh);
                    break;
                }
            }
        }

        // 2. Process Inline Style Attributes (Overwriting Rules)
        auto it = node.attributes.find("style");
        if (it != node.attributes.end() && !it->second.empty()) {
            // Parse this specific node's style text string as an inline declaration block
            auto inlineRules = Parse(it->second, true);
            for (const auto& rule : inlineRules) {
                ApplyDeclarations(rule.declarations, node, vw, vh);
            }
        }
    }

    for (auto& child : node.children)
        ApplyToTree(globalRules, *child, vw, vh);
}

void CSSParser::Apply(const std::vector<CSSRule>& rules, Node& root, int vw, int vh) {
    ApplyToTree(rules, root, vw, vh);
}