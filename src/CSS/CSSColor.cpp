//
// Created by tkdtu on 6/14/2026.
//
#include "CSSColor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

static TrieNode g_root;
static bool built = false;
const Color RGB(uint32_t hex) {
    uint8_t r = (hex >> 16) & 0xFF;
    uint8_t g = (hex >> 8)  & 0xFF;
    uint8_t b = (hex)       & 0xFF;
    return Color(r,g,b); // BGR
}

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

void BuildTrie() {
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

char toLower(char c) {
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}
static std::string_view Trim(std::string_view s) {
    while (!s.empty() && std::isspace((unsigned char)s.front()))
        s.remove_prefix(1);

    while (!s.empty() && std::isspace((unsigned char)s.back()))
        s.remove_suffix(1);

    return s;
}
static Color HSLToRGB(float h, float s, float l, float a = 1.0f) {
    h = std::fmod(h, 360.0f);
    if (h < 0.0f)
        h += 360.0f;

    s = std::clamp(s, 0.0f, 1.0f);
    l = std::clamp(l, 0.0f, 1.0f);

    float c = (1.0f - std::fabs(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = l - c * 0.5f;

    float r = 0, g = 0, b = 0;

    if (h < 60)       { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else              { r = c; b = x; }

    return Color(
        uint8_t((r + m) * 255.0f),
        uint8_t((g + m) * 255.0f),
        uint8_t((b + m) * 255.0f),
        uint8_t(a * 255.0f)
    );
}
std::optional<Color> ParseColor(std::string_view s) {
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
            return Color(e(s[1]), e(s[2]), e(s[3]));
        }

        if (s.size() == 7) {
            return Color(
                hex2(s[1], s[2]),
                hex2(s[3], s[4]),
                hex2(s[5], s[6])
            );
        }
    }
    auto startsWithIgnoreCase = [](std::string_view s, std::string_view prefix) {
        if (s.size() < prefix.size())
            return false;

        for (size_t i = 0; i < prefix.size(); ++i) {
            if (toLower(s[i]) != toLower(prefix[i]))
                return false;
        }
        return true;
    };

    auto parseFunctionArgs = [](std::string_view body) {
        std::vector<float> values;

        size_t pos = 0;
        while (pos < body.size()) {
            while (pos < body.size() &&
                  (std::isspace((unsigned char)body[pos]) ||
                   body[pos] == ','))
                ++pos;

            size_t start = pos;

            while (pos < body.size() &&
                   body[pos] != ',' &&
                   !std::isspace((unsigned char)body[pos]))
                ++pos;

            if (start == pos)
                break;

            std::string token(body.substr(start, pos - start));

            bool percent = false;
            if (!token.empty() && token.back() == '%') {
                percent = true;
                token.pop_back();
            }

            float v = std::stof(token);

            if (percent)
                v /= 100.0f;

            values.push_back(v);
        }

        return values;
    };
    if (startsWithIgnoreCase(s, "rgb(") ||
        startsWithIgnoreCase(s, "rgba("))
    {
        auto open = s.find('(');
        auto close = s.rfind(')');

        if (open == std::string_view::npos ||
            close == std::string_view::npos ||
            close <= open)
            return std::nullopt;

        auto args = parseFunctionArgs(
            s.substr(open + 1, close - open - 1)
        );

        if (args.size() != 3 && args.size() != 4)
            return std::nullopt;

        return Color(
            uint8_t(std::clamp(args[0], 0.0f, 255.0f)),
            uint8_t(std::clamp(args[1], 0.0f, 255.0f)),
            uint8_t(std::clamp(args[2], 0.0f, 255.0f)),
            uint8_t(args.size() == 4
                ? std::clamp(args[3], 0.0f, 1.0f) * 255.0f
                : 255.0f)
        );
    }
    if (startsWithIgnoreCase(s, "hsl(") ||
    startsWithIgnoreCase(s, "hsla("))
    {
        auto open = s.find('(');
        auto close = s.rfind(')');

        if (open == std::string_view::npos ||
            close == std::string_view::npos ||
            close <= open)
            return std::nullopt;

        auto args = parseFunctionArgs(
            s.substr(open + 1, close - open - 1)
        );

        if (args.size() != 3 && args.size() != 4)
            return std::nullopt;

        return HSLToRGB(
            args[0],                       // hue
            args[1],                       // saturation 0..1
            args[2],                       // lightness 0..1
            args.size() == 4 ? args[3] : 1.0f
        );
    }
    const TrieNode* node = &g_root;

    for (char c : s) {
        char lc = toLower(c);
        node = node->next[(unsigned char)lc];
        if (!node) return std::nullopt;
    }

    return node->value;
}
