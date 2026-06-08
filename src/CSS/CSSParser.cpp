#include "CSSParser.h"
#include "CSSTokenizer.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <iostream>
#include <optional>
#include <sstream>

// ── Selector parsing ────────────────────────────────────────────────────────

// Change parameter to std::string_view
constexpr uint32_t HashProperty(std::string_view str) {
    uint32_t hash = 5381;
    for (char c : str) {
        hash = ((hash << 5) + hash) + static_cast<uint32_t>(c);
    }
    return hash;
}

// ── Selector parsing ────────────────────────────────────────────────────────

CSSSelector CSSParser::ParseSelector(std::string_view s) {
    CSSSelector sel;

    size_t colon = s.find(':');
    if (colon != std::string_view::npos)
        s = s.substr(0, colon);

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
            sel.id = s.substr(i, j - i);
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
    bool inBlock = isInlineStyle;

    if (isInlineStyle) {
        CSSSelector universalSelector;
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
                if (inBlock && i + 1 < tokens.size() && tokens[i + 1].type == CSSTokenType::Value) {
                    current.declarations.push_back({ tok.value, tokens[i + 1].value });
                    i++;
                }
                break;
            default:
                break;
        }
    }

    if (isInlineStyle && !current.declarations.empty()) {
        rules.push_back(current);
    }

    return rules;
}

// ── Matching ─────────────────────────────────────────────────────────────────

bool CSSParser::Matches(const CSSSelector& sel, const Node& node) {
    if (node.type != NodeType::Element) return false;
    if (!sel.tag.empty() && sel.tag != node.tag) return false;

    // OPTIMIZATION: If your DOM parser extracts 'id' and 'class' fields into
    // the Node structure directly, use those instead of map lookups!
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
// Zero-allocation float parsing helper
static inline bool FastStringToFloat(std::string_view str, float& value) {
    auto result = std::from_chars(str.data(), str.data() + str.size(), value);
    return result.ec == std::errc();
}

static std::optional<CSSLength> ParseCSSLength(std::string_view val, int vw, int vh) {
    if (val.empty()) return std::nullopt;
    if (val == "auto") return CSSLength{ 0.0f, LengthUnit::Auto };

    size_t len = val.size();
    float num = 0.0f;

    if (val.back() == '%') {
        if (FastStringToFloat(val.substr(0, len - 1), num)) {
            return CSSLength{ num, LengthUnit::Percent };
        }
        return std::nullopt;
    }

    if (len > 2) {
        std::string_view unit = val.substr(len - 2);
        std::string_view num_part = val.substr(0, len - 2);

        if (unit == "px") {
            if (FastStringToFloat(num_part, num)) return CSSLength{ num, LengthUnit::Px };
        } else if (unit == "em") {
            if (FastStringToFloat(num_part, num)) return CSSLength{ num, LengthUnit::Em };
        } else if (unit == "vw") {
            // Note: Handle viewport scale context calculation outside or pass via signature if absolute px transformations are critical here
            if (FastStringToFloat(num_part, num)) return CSSLength{ num, LengthUnit::Vw };
        } else if (unit == "vh") {
            if (FastStringToFloat(num_part, num)) return CSSLength{ num, LengthUnit::Vh };
        }
    }

    if (FastStringToFloat(val, num)) {
        return CSSLength{ num, LengthUnit::Px };
    }

    return std::nullopt;
}


struct TrieNode {
    std::array<TrieNode*, 256> next{};
    std::optional<Color> value = std::nullopt;

    constexpr TrieNode() : next{} {}
};

// ---------- COLOR ENCODING ----------
static const Color RGB(uint32_t hex) {
    uint8_t r = (hex >> 16) & 0xFF;
    uint8_t g = (hex >> 8)  & 0xFF;
    uint8_t b = (hex)       & 0xFF;
    return Color(b, g, r); // BGR
}

// ---------- TRIE ROOT ----------
static TrieNode g_root;

// ---------- INSERT (compile-time friendly initializer) ----------
const void Insert(TrieNode& root, std::string_view key, Color color) {
    TrieNode* node = &root;

    for (char c : key) {
        auto uc = static_cast<unsigned char>(c);

        if (!node->next[uc]) {
            node->next[uc] = new TrieNode();
        }
        node = node->next[uc];
    }

    node->value = color;
}
static bool built = false;
// ---------- BUILD TABLE ----------
static void BuildTrie() {
    Insert(g_root, "aliceblue", RGB(0xF0F8FF));
    Insert(g_root, "antiquewhite", RGB(0xFAEBD7));
    Insert(g_root, "aqua", RGB(0x00FFFF));
    Insert(g_root, "aquamarine", RGB(0x7FFFD4));
    Insert(g_root, "azure", RGB(0xF0FFFF));
    Insert(g_root, "beige", RGB(0xF5F5DC));
    Insert(g_root, "bisque", RGB(0xFFE4C4));
    Insert(g_root, "black", RGB(0x000000));
    Insert(g_root, "blanchedalmond", RGB(0xFFEBCD));
    Insert(g_root, "blue", RGB(0x0000FF));
    Insert(g_root, "blueviolet", RGB(0x8A2BE2));
    Insert(g_root, "brown", RGB(0xA52A2A));
    Insert(g_root, "burlywood", RGB(0xDEB887));
    Insert(g_root, "cadetblue", RGB(0x5F9EA0));
    Insert(g_root, "chartreuse", RGB(0x7FFF00));
    Insert(g_root, "chocolate", RGB(0xD2691E));
    Insert(g_root, "coral", RGB(0xFF7F50));
    Insert(g_root, "cornflowerblue", RGB(0x6495ED));
    Insert(g_root, "cornsilk", RGB(0xFFF8DC));
    Insert(g_root, "crimson", RGB(0xDC143C));
    Insert(g_root, "cyan", RGB(0x00FFFF));
    Insert(g_root, "darkblue", RGB(0x00008B));
    Insert(g_root, "darkcyan", RGB(0x008B8B));
    Insert(g_root, "darkgoldenrod", RGB(0xB8860B));
    Insert(g_root, "darkgray", RGB(0xA9A9A9));
    Insert(g_root, "darkgrey", RGB(0xA9A9A9));
    Insert(g_root, "darkgreen", RGB(0x006400));
    Insert(g_root, "darkkhaki", RGB(0xBDB76B));
    Insert(g_root, "darkmagenta", RGB(0x8B008B));
    Insert(g_root, "darkolivegreen", RGB(0x556B2F));
    Insert(g_root, "darkorange", RGB(0xFF8C00));
    Insert(g_root, "darkorchid", RGB(0x9932CC));
    Insert(g_root, "darkred", RGB(0x8B0000));
    Insert(g_root, "darksalmon", RGB(0xE9967A));
    Insert(g_root, "darkseagreen", RGB(0x8FBC8F));
    Insert(g_root, "darkslateblue", RGB(0x483D8B));
    Insert(g_root, "darkslategray", RGB(0x2F4F4F));
    Insert(g_root, "darkslategrey", RGB(0x2F4F4F));
    Insert(g_root, "darkturquoise", RGB(0x00CED1));
    Insert(g_root, "darkviolet", RGB(0x9400D3));
    Insert(g_root, "deeppink", RGB(0xFF1493));
    Insert(g_root, "deepskyblue", RGB(0x00BFFF));
    Insert(g_root, "dimgray", RGB(0x696969));
    Insert(g_root, "dimgrey", RGB(0x696969));
    Insert(g_root, "dodgerblue", RGB(0x1E90FF));
    Insert(g_root, "firebrick", RGB(0xB22222));
    Insert(g_root, "floralwhite", RGB(0xFFFAF0));
    Insert(g_root, "forestgreen", RGB(0x228B22));
    Insert(g_root, "fuchsia", RGB(0xFF00FF));
    Insert(g_root, "gainsboro", RGB(0xDCDCDC));
    Insert(g_root, "ghostwhite", RGB(0xF8F8FF));
    Insert(g_root, "gold", RGB(0xFFD700));
    Insert(g_root, "goldenrod", RGB(0xDAA520));
    Insert(g_root, "gray", RGB(0x808080));
    Insert(g_root, "grey", RGB(0x808080));
    Insert(g_root, "green", RGB(0x008000));
    Insert(g_root, "greenyellow", RGB(0xADFF2F));
    Insert(g_root, "honeydew", RGB(0xF0FFF0));
    Insert(g_root, "hotpink", RGB(0xFF69B4));
    Insert(g_root, "indianred", RGB(0xCD5C5C));
    Insert(g_root, "indigo", RGB(0x4B0082));
    Insert(g_root, "ivory", RGB(0xFFFFF0));
    Insert(g_root, "khaki", RGB(0xF0E68C));
    Insert(g_root, "lavender", RGB(0xE6E6FA));
    Insert(g_root, "lavenderblush", RGB(0xFFF0F5));
    Insert(g_root, "lawngreen", RGB(0x7CFC00));
    Insert(g_root, "lemonchiffon", RGB(0xFFFACD));
    Insert(g_root, "lightblue", RGB(0xADD8E6));
    Insert(g_root, "lightcoral", RGB(0xF08080));
    Insert(g_root, "lightcyan", RGB(0xE0FFFF));
    Insert(g_root, "lightgoldenrodyellow", RGB(0xFAFAD2));
    Insert(g_root, "lightgray", RGB(0xD3D3D3));
    Insert(g_root, "lightgrey", RGB(0xD3D3D3));
    Insert(g_root, "lightgreen", RGB(0x90EE90));
    Insert(g_root, "lightpink", RGB(0xFFB6C1));
    Insert(g_root, "lightsalmon", RGB(0xFFA07A));
    Insert(g_root, "lightseagreen", RGB(0x20B2AA));
    Insert(g_root, "lightskyblue", RGB(0x87CEFA));
    Insert(g_root, "lightgray", RGB(0x778899));
    Insert(g_root, "lightslategray", RGB(0x778899));
    Insert(g_root, "lightslategrey", RGB(0x778899));
    Insert(g_root, "lightsteelblue", RGB(0xB0C4DE));
    Insert(g_root, "lightyellow", RGB(0xFFFFE0));
    Insert(g_root, "lime", RGB(0x00FF00));
    Insert(g_root, "limegreen", RGB(0x32CD32));
    Insert(g_root, "linen", RGB(0xFAF0E6));
    Insert(g_root, "magenta", RGB(0xFF00FF));
    Insert(g_root, "maroon", RGB(0x800000));
    Insert(g_root, "mediumaquamarine", RGB(0x66CDAA));
    Insert(g_root, "mediumblue", RGB(0x0000CD));
    Insert(g_root, "mediumorchid", RGB(0xBA55D3));
    Insert(g_root, "mediumpurple", RGB(0x9370DB));
    Insert(g_root, "mediumseagreen", RGB(0x3CB371));
    Insert(g_root, "mediumslateblue", RGB(0x7B68EE));
    Insert(g_root, "mediumspringgreen", RGB(0x00FA9A));
    Insert(g_root, "mediumturquoise", RGB(0x48D1CC));
    Insert(g_root, "mediumvioletred", RGB(0xC71585));
    Insert(g_root, "midnightblue", RGB(0x191970));
    Insert(g_root, "mintcream", RGB(0xF5FFFA));
    Insert(g_root, "mistyrose", RGB(0xFFE4E1));
    Insert(g_root, "moccasin", RGB(0xFFE4B5));
    Insert(g_root, "navajowhite", RGB(0xFFDEAD));
    Insert(g_root, "navy", RGB(0x000080));
    Insert(g_root, "oldlace", RGB(0xFDF5E6));
    Insert(g_root, "olive", RGB(0x808000));
    Insert(g_root, "olivedrab", RGB(0x6B8E23));
    Insert(g_root, "orange", RGB(0xFFA500));
    Insert(g_root, "orangered", RGB(0xFF4500));
    Insert(g_root, "orchid", RGB(0xDA70D6));
    Insert(g_root, "palegoldenrod", RGB(0xEEE8AA));
    Insert(g_root, "palegreen", RGB(0x98FB98));
    Insert(g_root, "paleturquoise", RGB(0xAFEEEE));
    Insert(g_root, "palevioletred", RGB(0xDB7093));
    Insert(g_root, "papayawhip", RGB(0xFFEFD5));
    Insert(g_root, "peachpuff", RGB(0xFFDAB9));
    Insert(g_root, "peru", RGB(0xCD853F));
    Insert(g_root, "pink", RGB(0xFFC0CB));
    Insert(g_root, "plum", RGB(0xDDA0DD));
    Insert(g_root, "powderblue", RGB(0xB0E0E6));
    Insert(g_root, "purple", RGB(0x800080));
    Insert(g_root, "rebeccapurple", RGB(0x663399));
    Insert(g_root, "red", RGB(0xFF0000));
    Insert(g_root, "rosybrown", RGB(0xBC8F8F));
    Insert(g_root, "royalblue", RGB(0x4169E1));
    Insert(g_root, "saddlebrown", RGB(0x8B4513));
    Insert(g_root, "salmon", RGB(0xFA8072));
    Insert(g_root, "sandybrown", RGB(0xF4A460));
    Insert(g_root, "seagreen", RGB(0x2E8B57));
    Insert(g_root, "seashell", RGB(0xFFF5EE));
    Insert(g_root, "sienna", RGB(0xA0522D));
    Insert(g_root, "silver", RGB(0xC0C0C0));
    Insert(g_root, "skyblue", RGB(0x87CEEB));
    Insert(g_root, "slateblue", RGB(0x6A5ACD));
    Insert(g_root, "slategray", RGB(0x708090));
    Insert(g_root, "slategrey", RGB(0x708090));
    Insert(g_root, "snow", RGB(0xFFFAFA));
    Insert(g_root, "springgreen", RGB(0x00FF7F));
    Insert(g_root, "steelblue", RGB(0x4682B4));
    Insert(g_root, "tan", RGB(0xD2B48C));
    Insert(g_root, "teal", RGB(0x008080));
    Insert(g_root, "thistle", RGB(0xD8BFD8));
    Insert(g_root, "tomato", RGB(0xFF6347));
    Insert(g_root, "turquoise", RGB(0x40E0D0));
    Insert(g_root, "violet", RGB(0xEE82EE));
    Insert(g_root, "wheat", RGB(0xF5DEB3));
    Insert(g_root, "white", RGB(0xFFFFFF));
    Insert(g_root, "whitesmoke", RGB(0xF5F5F5));
    Insert(g_root, "yellow", RGB(0xFFFF00));
    Insert(g_root, "yellowgreen", RGB(0x9ACD32));
}

// ---------- NORMALIZATION ----------
static inline char toLower(char c) {
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

// ---------- FAST TRIE PARSER ----------
static std::optional<Color> ParseColor(std::string_view s) {
    if (!built)
    {
        built = true;
        BuildTrie();
    }
    if (s.empty()) return std::nullopt;

    // hex path first (fast reject)
    if (s[0] == '#') {
        auto hex = [](char c)->uint8_t {
            if (c >= '0' && c <= '9') return c - '0';
            return (c & 0xF) + 9; // fast path (assumes valid input)
        };

        auto hex2 = [&](char a, char b) {
            return (hex(a) << 4) | hex(b);
        };

        if (s.size() == 4) {
            auto e = [&](char c) { uint8_t v = hex(c); return v * 17; };
            return Color(e(s[3]), e(s[2]), e(s[1]));
        }

        if (s.size() == 7) {
            return Color(
                hex2(s[1], s[2]),
                hex2(s[3], s[4]),
                hex2(s[5], s[6])
            );
        }
    }

    const TrieNode* node = &g_root;

    for (char c : s) {
        char lc = toLower(c);
        node = node->next[(unsigned char)lc];
        if (!node) return std::nullopt;
    }

    return node->value;
}
static std::optional<BorderStyle> ParseBorderStyle(std::string_view val) {
    if (val == "solid")  return BorderStyle::Solid;
    if (val == "none")   return BorderStyle::None;
    if (val == "dotted") return BorderStyle::Dotted;
    if (val == "dashed") return BorderStyle::Dashed;
    return std::nullopt;
}
#define CSS_DEBUG
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
    std::vector<std::string_view> parts;
    parts.reserve(8); // Avoid reallocation inside layout splits

    for (const auto& d : decls)
    {
        std::string_view p = d.property;
        std::string_view v = d.value;
        parts.clear();

        // ── color / background ───────────────────────────────────────────────
        switch (HashProperty(p)) {
             case HashProperty("background"):
                case HashProperty("background-color"):
        {
            auto c = ParseColor(v);
            if (!c) CSS_WARN("Failed to parse background color: \"" << v << "\"");
            node.specifiedStyle.set.background = true;
            node.specifiedStyle.hasBackground = true;
            node.specifiedStyle.backgroundColor = c.value_or(Color(0, 0, 0));
        }break;

        // ── border shorthand ─────────────────────────────────────────────────
        case HashProperty("border"): {
            SplitShorthand(v, parts);

            for (auto& c : parts) {
                auto widthVal = ParseCSSLength(c, vw, vh);
                if (widthVal.has_value()) {
                    node.specifiedStyle.borderTop.borderWidth    = widthVal.value();
                    node.specifiedStyle.borderBottom.borderWidth = widthVal.value();
                    node.specifiedStyle.borderLeft.borderWidth   = widthVal.value();
                    node.specifiedStyle.borderRight.borderWidth  = widthVal.value();
                    continue;
                }

                if (auto style = ParseBorderStyle(c)) {
                    node.specifiedStyle.borderTop.borderStyle    = *style;
                    node.specifiedStyle.borderBottom.borderStyle = *style;
                    node.specifiedStyle.borderLeft.borderStyle   = *style;
                    node.specifiedStyle.borderRight.borderStyle  = *style;
                    continue;
                }

                if (auto color = ParseColor(c)) {
                    node.specifiedStyle.borderTop.borderColor    = *color;
                    node.specifiedStyle.borderBottom.borderColor = *color;
                    node.specifiedStyle.borderLeft.borderColor   = *color;
                    node.specifiedStyle.borderRight.borderColor  = *color;
                    continue;
                }

                CSS_WARN("Unrecognized token in border shorthand: \"" << c << "\"");
            }
        }break;

        // ── border direction longhands ────────────────────────────────────────
        case HashProperty("border-top"):
            case HashProperty("border-bottom"):
            case HashProperty("border-left"):
            case HashProperty("border-right"): {
            SplitShorthand(v, parts);

            auto* b = &node.specifiedStyle.borderTop;
            if (p == "border-bottom") b = &node.specifiedStyle.borderBottom;
            if (p == "border-left")   b = &node.specifiedStyle.borderLeft;
            if (p == "border-right")  b = &node.specifiedStyle.borderRight;

            for (auto& c : parts) {
                auto w = ParseCSSLength(c, vw, vh);
                if (w.has_value()) { b->borderWidth = w.value(); continue; }

                if (auto style = ParseBorderStyle(c)) { b->borderStyle = *style; continue; }
                if (auto color = ParseColor(c))       { b->borderColor = *color; continue; }

                CSS_WARN("Unrecognized token in " << p << " shorthand: \"" << c << "\"");
            }
        }break;

        // ── border component longhands ────────────────────────────────────────
        case HashProperty("border-width"): {
            auto w = ParseCSSLength(v, vw, vh);
            if (w.has_value()) {
                node.specifiedStyle.borderTop.borderWidth    = w.value();
                node.specifiedStyle.borderBottom.borderWidth = w.value();
                node.specifiedStyle.borderLeft.borderWidth   = w.value();
                node.specifiedStyle.borderRight.borderWidth  = w.value();
            } else {
                CSS_WARN("Failed to parse border-width value: \"" << v << "\"");
            }
        }break;
        case HashProperty("border-style"): {
            auto s = ParseBorderStyle(v);
            if (!s) CSS_WARN("Failed to parse border-style value: \"" << v << "\"");
            BorderStyle bs = s.value_or(BorderStyle::None);
            node.specifiedStyle.borderTop.borderStyle    = bs;
            node.specifiedStyle.borderBottom.borderStyle = bs;
            node.specifiedStyle.borderLeft.borderStyle   = bs;
            node.specifiedStyle.borderRight.borderStyle  = bs;
        }break;
        case HashProperty("border-color"): {
            auto c = ParseColor(v);
            if (!c) CSS_WARN("Failed to parse border-color value: \"" << v << "\"");
            Color bc = c.value_or(Color(0,0,0));
            node.specifiedStyle.borderTop.borderColor    = bc;
            node.specifiedStyle.borderBottom.borderColor = bc;
            node.specifiedStyle.borderLeft.borderColor   = bc;
            node.specifiedStyle.borderRight.borderColor  = bc;
        }break;
        case HashProperty("color"):
        {
            auto c = ParseColor(v);
            if (!c) CSS_WARN("Failed to parse color value: \"" << v << "\"");
            node.specifiedStyle.set.color = true;
            node.specifiedStyle.color = c.value_or(Color(0,0,0));
        }break;

        // ── font ─────────────────────────────────────────────────────────────

        case HashProperty("font-size"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) {
                node.specifiedStyle.set.font_size = true;
                node.specifiedStyle.font_size = *val;
            } else {
                CSS_WARN("Failed to parse font-size value: \"" << v << "\"");
            }
        }break;
        case HashProperty("font-weight"):
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
        }break;
        case HashProperty("font-style"):
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
        }break;

        // ── margin shorthand ────────────────────────────────────────────────

        case HashProperty("margin"):
        {
            SplitShorthand(v, parts);

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
        }break;

        // ── margin longhands ────────────────────────────────────────────────

        case HashProperty("margin-top"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.margin_top = true; node.specifiedStyle.margin_top = *val; }
            else CSS_WARN("Failed to parse margin-top value: \"" << v << "\"");
        }break;
        case HashProperty("margin-bottom"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.margin_bottom = true; node.specifiedStyle.margin_bottom = *val; }
            else CSS_WARN("Failed to parse margin-bottom value: \"" << v << "\"");
        }break;
        case HashProperty("margin-left"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.margin_left = true; node.specifiedStyle.margin_left = *val; }
            else CSS_WARN("Failed to parse margin-left value: \"" << v << "\"");
        }break;
        case HashProperty("margin-right"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.margin_right = true; node.specifiedStyle.margin_right = *val; }
            else CSS_WARN("Failed to parse margin-right value: \"" << v << "\"");
        }break;

        // ── padding shorthand ─────────────────────────────────────────────────

        case HashProperty("padding"):
        {
            SplitShorthand(v, parts);

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
        }break;

        // ── padding longhands ───────────────────────────────────────────────

        case HashProperty("padding-top"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.padding_top = true; node.specifiedStyle.padding_top = *val; }
            else CSS_WARN("Failed to parse padding-top value: \"" << v << "\"");
        }break;
        case HashProperty("padding-bottom"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.padding_bottom = true; node.specifiedStyle.padding_bottom = *val; }
            else CSS_WARN("Failed to parse padding-bottom value: \"" << v << "\"");
        }break;
        case HashProperty("padding-left"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.padding_left = true; node.specifiedStyle.padding_left = *val; }
            else CSS_WARN("Failed to parse padding-left value: \"" << v << "\"");
        }break;
        case HashProperty("padding-right"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) { node.specifiedStyle.set.padding_right = true; node.specifiedStyle.padding_right = *val; }
            else CSS_WARN("Failed to parse padding-right value: \"" << v << "\"");
        }break;

        // ── size ────────────────────────────────────────────────────────────

        case HashProperty("width"):
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.width = true; node.specifiedStyle.width = *parsed; }
            else CSS_WARN("Failed to parse width value: \"" << v << "\"");
        }break;
        case HashProperty("min-width"):
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.min_width = true; node.specifiedStyle.min_width = *parsed; }
            else CSS_WARN("Failed to parse min-width value: \"" << v << "\"");
        }break;
        case HashProperty("max-width"):
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.max_width = true; node.specifiedStyle.max_width = *parsed; }
            else CSS_WARN("Failed to parse max-width value: \"" << v << "\"");
        }break;
        case HashProperty("height"):
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.height = true; node.specifiedStyle.height = *parsed; }
            else CSS_WARN("Failed to parse height value: \"" << v << "\"");
        }break;
        case HashProperty("min-height"):
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.min_height = true; node.specifiedStyle.min_height = *parsed; }
            else CSS_WARN("Failed to parse min-height value: \"" << v << "\"");
        }break;
        case HashProperty("max-height"):
        {
            auto parsed = ParseCSSLength(v, vw, vh);
            if (parsed) { node.specifiedStyle.set.max_height = true; node.specifiedStyle.max_height = *parsed; }
            else CSS_WARN("Failed to parse max-height value: \"" << v << "\"");

        }break;

        // ── text alignment ────────────────────────────────────────────────

        case HashProperty("text-align"):
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
            }break;
        }
// ── border radius shorthand & longhands ───────────────────────────────

        case HashProperty("border-radius"):
        {
            SplitShorthand(v, parts);

            CSSLength zeroLen = { 0.0f, LengthUnit::Px };
            CSSLength tl = zeroLen, tr = zeroLen, br = zeroLen, bl = zeroLen;
            bool parsed_any = false;

            if (parts.size() == 1) {
                // e.g., border-radius: 10px;
                auto val = ParseCSSLength(parts[0], vw, vh);
                if (val) { tl = tr = br = bl = *val; parsed_any = true; }
                else CSS_WARN("Failed to parse border-radius value: \"" << parts[0] << "\"");
            }
            else if (parts.size() == 2) {
                // e.g., border-radius: 10px 20px;
                auto tl_br = ParseCSSLength(parts[0], vw, vh);
                auto tr_bl = ParseCSSLength(parts[1], vw, vh);
                if (tl_br && tr_bl) {
                    tl = br = *tl_br;
                    tr = bl = *tr_bl;
                    parsed_any = true;
                }
            }
            else if (parts.size() == 3) {
                // e.g., border-radius: 10px 20px 30px;
                auto top_left     = ParseCSSLength(parts[0], vw, vh);
                auto top_r_bot_l  = ParseCSSLength(parts[1], vw, vh);
                auto bottom_right = ParseCSSLength(parts[2], vw, vh);
                if (top_left && top_r_bot_l && bottom_right) {
                    tl = *top_left;
                    tr = bl = *top_r_bot_l;
                    br = *bottom_right;
                    parsed_any = true;
                }
            }
            else if (parts.size() == 4) {
                // e.g., border-radius: 10px 20px 30px 40px;
                auto top_left     = ParseCSSLength(parts[0], vw, vh);
                auto top_right    = ParseCSSLength(parts[1], vw, vh);
                auto bottom_right = ParseCSSLength(parts[2], vw, vh);
                auto bottom_left  = ParseCSSLength(parts[3], vw, vh);
                if (top_left && top_right && bottom_right && bottom_left) {
                    tl = *top_left; tr = *top_right; br = *bottom_right; bl = *bottom_left;
                    parsed_any = true;
                }
            }

            if (parsed_any) {
                node.specifiedStyle.set.border_radius_top_left     = true;
                node.specifiedStyle.set.border_radius_top_right    = true;
                node.specifiedStyle.set.border_radius_bottom_right = true;
                node.specifiedStyle.set.border_radius_bottom_left  = true;

                node.specifiedStyle.border_radius_top_left     = tl;
                node.specifiedStyle.border_radius_top_right    = tr;
                node.specifiedStyle.border_radius_bottom_right = br;
                node.specifiedStyle.border_radius_bottom_left  = bl;
            }
        }break;

        case HashProperty("border-top-left-radius"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) {
                node.specifiedStyle.set.border_radius_top_left = true;
                node.specifiedStyle.border_radius_top_left = *val;
            }
        }break;

        case HashProperty("border-top-right-radius"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) {
                node.specifiedStyle.set.border_radius_top_right = true;
                node.specifiedStyle.border_radius_top_right = *val;
            }
        }break;

        case HashProperty("border-bottom-right-radius"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) {
                node.specifiedStyle.set.border_radius_bottom_right = true;
                node.specifiedStyle.border_radius_bottom_right = *val;
            }
        }break;

        case HashProperty("border-bottom-left-radius"):
        {
            auto val = ParseCSSLength(v, vw, vh);
            if (val) {
                node.specifiedStyle.set.border_radius_bottom_left = true;
                node.specifiedStyle.border_radius_bottom_left = *val;
            }
        }break;
        // ── text decoration ───────────────────────────────────────────────

        case HashProperty("text-decoration"):
        {
            SplitShorthand(v, parts);

            for (const auto& w : parts)
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
                    node.specifiedStyle.textDecoration = TextDecoration::Blink;
                else if (w == "spelling-error")
                    node.specifiedStyle.textDecoration = TextDecoration::SpellingError;
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
                    node.specifiedStyle.textDecorationThickness = *lenVal;
                else if (auto colVal = ParseColor(w))
                    node.specifiedStyle.textDecorationColor = *colVal;
                else
                    CSS_WARN("Unrecognized token in text-decoration: \"" << w << "\"");
            }break;
        }

        // ── layout ──────────────────────────────────────────────────────────

        case HashProperty("display"):
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
            }break;
        }
        case HashProperty("box-sizing"):
        {
            node.specifiedStyle.set.boxSizing = true;
            if (v == "border-box")
                node.specifiedStyle.boxSizing = BoxSizing::BorderBox;
            else if (v == "content-box")
                node.specifiedStyle.boxSizing = BoxSizing::ContentBox;
            else
                CSS_WARN("Unrecognized box-sizing value: \"" << v << "\"");

            break;
        }
        case HashProperty("white-space"):
        {
            node.specifiedStyle.set.whiteSpace = true;
            if (v == "nowrap")
                node.specifiedStyle.whiteSpace = WhiteSpace::NoWrap;
            else if (v == "normal")
                node.specifiedStyle.whiteSpace = WhiteSpace::Normal;
            else {
                CSS_WARN("Unrecognized white-space value: \"" << v << "\" (defaulting to normal)");
                node.specifiedStyle.whiteSpace = WhiteSpace::Normal;
            }break;
        }
        case HashProperty("text-overflow"):
        {
            node.specifiedStyle.set.textOverflow = true;
            if (v == "ellipsis")
                node.specifiedStyle.textOverflow = TextOverflow::Ellipsis;
            else if (v == "clip")
                node.specifiedStyle.textOverflow = TextOverflow::Clip;
            else {
                CSS_WARN("Unrecognized text-overflow value: \"" << v << "\" (defaulting to clip)");
                node.specifiedStyle.textOverflow = TextOverflow::Clip;
            }break;
        }

        // ── images ───────────────────────────────────────────────────────────

        case HashProperty("object-fit"):
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
            }break;
        }
        case HashProperty("vertical-align"): {
            node.specifiedStyle.set.verticalAlign = true;
            if (v == "baseline")
                node.specifiedStyle.verticalAlign = VerticalAlignKeyword::Baseline;
            else if (v == "sub")
                node.specifiedStyle.verticalAlign = VerticalAlignKeyword::Sub;
            else if (v == "super")
                node.specifiedStyle.verticalAlign = VerticalAlignKeyword::Super;
            else if (v == "top")
                node.specifiedStyle.verticalAlign = VerticalAlignKeyword::Top;
            else if (v == "text-top")
                node.specifiedStyle.verticalAlign = VerticalAlignKeyword::TextTop;
            else if (v == "middle")
                node.specifiedStyle.verticalAlign = VerticalAlignKeyword::Middle;
            else if (v == "bottom")
                node.specifiedStyle.verticalAlign = VerticalAlignKeyword::Bottom;
            else if (v == "text-bottom")
                node.specifiedStyle.verticalAlign = VerticalAlignKeyword::TextBottom;
            else if (ParseCSSLength(v, vw, vh).has_value()) {
                node.specifiedStyle.verticalAlign = VerticalAlignKeyword::Other;
                node.specifiedStyle.verticalAlignValue = ParseCSSLength(v, vw, vh).value();
            }
                 break;
        }
        // ── unrecognized ─────────────────────────────────────────────────────

            default:
        {
            CSS_WARN("Unrecognized CSS property: \"" << p << "\": \"" << v << "\"");
                 break;
        }
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