#include "CSSParser.h"
#include "CSSTokenizer.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

// ── Selector parsing ────────────────────────────────────────────────────────

CSSSelector CSSParser::ParseSelector(const std::string& raw) {
    CSSSelector sel;

    // strip pseudo-class (":link", ":visited", ":hover" etc)
    std::string s = raw;
    size_t colon = s.find(':');
    if (colon != std::string::npos)
        s = s.substr(0, colon);

    // trim
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end   = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return sel;
    s = s.substr(start, end - start + 1);

    // parse tag, #id, .class in one pass
    // e.g. "div.container", "#header", "a.active"
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

// ── Rule parsing ─────────────────────────────────────────────────────────────

std::vector<CSSRule> CSSParser::Parse(const std::string& css) {
    CSSTokenizer tokenizer;
    auto tokens = tokenizer.Tokenize(css);

    std::vector<CSSRule> rules;
    CSSRule current;
    bool inBlock = false;

    for (size_t i = 0; i < tokens.size(); i++) {
        const CSSToken& tok = tokens[i];

        switch (tok.type) {
            case CSSTokenType::Selector:
                current.selectors.push_back(ParseSelector(tok.value));
                break;

            case CSSTokenType::Comma:
                // next selector token will be added to same rule
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
                    i++; // consume the value token
                }
                break;

            default:
                break;
        }
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
        // class attribute can be space-separated — check each token
        std::istringstream ss(it->second);
        std::string token;
        bool found = false;
        while (ss >> token) {
            if (token == sel.cls) { found = true; break; }
        }
        if (!found) return false;
    }

    return true;
}



// ── Value application ────────────────────────────────────────────────────────

static Color ParseColor(const std::string& val) {
    if (val.empty()) return Color(0, 0, 0);

    // #rgb shorthand
    if (val[0] == '#' && val.size() == 4) {
        auto expand = [](char c) -> uint8_t {
            int v = std::isdigit(c) ? c - '0' : std::tolower(c) - 'a' + 10;
            return static_cast<uint8_t>(v * 17);
        };
        return Color(expand(val[3]), expand(val[2]), expand(val[1])); // BGR
    }

    // #rrggbb
    if (val[0] == '#' && val.size() == 7) {
        auto hex2 = [&](size_t i) -> uint8_t {
            return static_cast<uint8_t>(std::stoi(val.substr(i, 2), nullptr, 16));
        };
        return Color(hex2(5), hex2(3), hex2(1)); // B, G, R
    }

    // named colors — just the common ones for now
    if (val == "white") return Color(255, 255, 255); // same either way
    if (val == "black") return Color(0,   0,   0);   // same either way
    if (val == "red")   return Color(0,   0,   255); // B=0, G=0, R=255 → stored as BGR
    if (val == "green") return Color(0,   128, 0);   // same
    if (val == "blue")  return Color(255, 0,   0);   // B=255 first
    if (val == "gray" || val == "grey") return Color(128, 128, 128); // same

    return Color(0, 0, 0); // fallback
}

static int ParseValue(const std::string& val, int vw, int vh) {
    if (val.empty()) return 0;
    try {
        if (val.size() > 2 && val.substr(val.size() - 2) == "vw")
            return static_cast<int>(std::stof(val) / 100.0f * vw);
        if (val.size() > 2 && val.substr(val.size() - 2) == "vh")
            return static_cast<int>(std::stof(val) / 100.0f * vh);
        if (val.size() > 2 && val.substr(val.size() - 2) == "px")
            return static_cast<int>(std::stof(val));
        // don't handle em here — return a sentinel
        if (val.size() > 2 && val.substr(val.size() - 2) == "em")
            return -3; // sentinel: "em unit, needs resolution"
        if (val == "auto") return -2; // sentinel for auto

        return std::stoi(val);
    } catch (...) { return 0; }
}
void CSSParser::ApplyDeclarations(const std::vector<CSSDeclaration> &decls, Node &node, int vw,
    int vh) {
    for (const auto& d : decls) {
        const std::string& p = d.property;
        const std::string& v = d.value;

        if (p == "background" || p == "background-color") {
            node.specifiedStyle.backgroundColor = ParseColor(v);
            node.specifiedStyle.hasBackground = true;
        }
        else if (p == "color") {
            node.specifiedStyle.color = ParseColor(v);
        }
        else if (p == "font-size") {
            if (v.size() > 2 && v.substr(v.size() - 2) == "em") {
                node.specifiedStyle.font_size_em = std::stof(v);
                node.specifiedStyle.font_size = 0; // don't use the px path
            } else {
                node.specifiedStyle.font_size = ParseValue(v, vw, vh);
            }
        }
        else if (p == "font-weight") {
            node.specifiedStyle.font_bold = (v == "bold" || v == "700" || v == "800" || v == "900");
        }
        if (p == "margin") {

            // split into tokens
            std::vector<std::string> parts;
            std::istringstream ss(v);
            std::string part;
            while (ss >> part) parts.push_back(part);

            int top = 0, right = 0, bottom = 0, left = 0;
            if (parts.size() == 1) {
                top = right = bottom = left = ParseValue(parts[0], vw, vh);
            } else if (parts.size() == 2) {
                top = bottom = ParseValue(parts[0], vw, vh);
                right = left = ParseValue(parts[1], vw, vh);
            } else if (parts.size() == 4) {
                top    = ParseValue(parts[0], vw, vh);
                right  = ParseValue(parts[1], vw, vh);
                bottom = ParseValue(parts[2], vw, vh);
                left   = ParseValue(parts[3], vw, vh);
            }

            if (top    != -2) node.specifiedStyle.margin_top    = top;
            if (bottom != -2) node.specifiedStyle.margin_bottom = bottom;
            if (left   != -2) node.specifiedStyle.margin_left   = left;
            if (right  != -2) node.specifiedStyle.margin_right  = right;

            // store auto flags for horizontal centering
            node.specifiedStyle.margin_left_auto  = (parts.size() >= 2 && parts[1] == "auto");
            node.specifiedStyle.margin_right_auto = (parts.size() >= 2 && parts[1] == "auto");
        }
        else if (p == "margin-top")    node.specifiedStyle.margin_top    = ParseValue(v, vw, vh);
        else if (p == "margin-bottom") node.specifiedStyle.margin_bottom = ParseValue(v, vw, vh);
        else if (p == "margin-left")   node.specifiedStyle.margin_left   = ParseValue(v, vw, vh);
        else if (p == "margin-right")  node.specifiedStyle.margin_right  = ParseValue(v, vw, vh);
        else if (p == "padding")       {
            int px = ParseValue(v, vw, vh);
            node.specifiedStyle.padding_top    = px;
            node.specifiedStyle.padding_bottom = px;
            node.specifiedStyle.padding_left   = px;
            node.specifiedStyle.padding_right  = px;
        }
        else if (p == "padding-top")    node.specifiedStyle.padding_top    = ParseValue(v, vw, vh);
        else if (p == "padding-bottom") node.specifiedStyle.padding_bottom = ParseValue(v, vw, vh);
        else if (p == "padding-left")   node.specifiedStyle.padding_left   = ParseValue(v, vw, vh);
        else if (p == "padding-right")  node.specifiedStyle.padding_right  = ParseValue(v, vw, vh);
        else if (p == "width")          node.specifiedStyle.width          = ParseValue(v, vw, vh);
        else if (p == "height")         node.specifiedStyle.height         = ParseValue(v, vw, vh);
        else if (p == "min-height") node.specifiedStyle.min_height = ParseValue(v, vw, vh);
        else if (p == "text-align") {
            if (v == "center") node.specifiedStyle.textAlign = TextAlign::Center;
            else if (v == "right")  node.specifiedStyle.textAlign = TextAlign::Right;
            else                    node.specifiedStyle.textAlign = TextAlign::Left;
        }
    }
}

// ── Tree traversal ───────────────────────────────────────────────────────────

// CSSParser.cpp
void CSSParser::ApplyToTree(const std::vector<CSSRule>& rules, Node& node, int vw, int vh) {
    if (node.type == NodeType::Element) {
        for (const auto& rule : rules) {
            for (const auto& sel : rule.selectors) {
                if (Matches(sel, node)) {
                    ApplyDeclarations(rule.declarations, node, vw, vh);
                    break;
                }
            }
        }
    }
    for (auto& child : node.children)
        ApplyToTree(rules, *child, vw, vh);
}

void CSSParser::Apply(const std::vector<CSSRule>& rules, Node& root, int vw, int vh) {
    ApplyToTree(rules, root, vw, vh);
}