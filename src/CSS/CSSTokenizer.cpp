#include "CSSTokenizer.h"
#include <cctype>
#include <algorithm>
#include <string_view>

std::vector<CSSToken> CSSTokenizer::Tokenize(const std::string& css, bool isInlineStyle) {
    std::vector<CSSToken> tokens;

    // Speculatively reserve space to avoid repeated vector reallocations
    tokens.reserve(css.size() / 10);

    size_t i = 0;
    size_t len = css.size();
    bool inBlock = isInlineStyle;

    // Fast, zero-allocation whitespace trimmer using string_view
    auto cleanStringView = [](std::string_view sv) -> std::string_view {
        size_t first = sv.find_first_not_of(" \t\n\r");
        if (first == std::string_view::npos) return {};
        size_t last = sv.find_last_not_of(" \t\n\r");
        return sv.substr(first, (last - first + 1));
    };

    auto skipWhitespace = [&]() {
        while (i < len && std::isspace(static_cast<unsigned char>(css[i]))) {
            i++;
        }
    };

    while (i < len) {
        skipWhitespace();
        if (i >= len) break;

        char c = css[i];

        if (!inBlock) {
            if (c == '{') {
                tokens.push_back({ CSSTokenType::OpenBrace, "{" });
                inBlock = true;
                i++;
            } else if (c == ',') {
                tokens.push_back({ CSSTokenType::Comma, "," });
                i++;
            } else {
                // Read Selector quickly using a scan loop
                size_t start = i;
                while (i < len && css[i] != '{' && css[i] != ',') {
                    i++;
                }
                std::string_view sel = cleanStringView(std::string_view(css).substr(start, i - start));
                if (!sel.empty()) {
                    tokens.push_back({ CSSTokenType::Selector, std::string(sel) });
                }
            }
        } else {
            if (!isInlineStyle && c == '}') {
                tokens.push_back({ CSSTokenType::CloseBrace, "}" });
                inBlock = false;
                i++;
            } else if (c == ';') {
                tokens.push_back({ CSSTokenType::Semicolon, ";" });
                i++;
            } else {
                // Read Property Name
                size_t propStart = i;
                while (i < len && css[i] != ':' && css[i] != ';' && css[i] != '}') {
                    i++;
                }
                std::string_view prop = cleanStringView(std::string_view(css).substr(propStart, i - propStart));

                skipWhitespace();
                if (i < len && css[i] == ':') {
                    i++; // Skip the colon splitter
                    skipWhitespace();

                    // Read Value
                    size_t valStart = i;
                    while (i < len && css[i] != ';' && css[i] != '}') {
                        i++;
                    }
                    std::string_view val = cleanStringView(std::string_view(css).substr(valStart, i - valStart));

                    if (!prop.empty() && !val.empty()) {
                        tokens.push_back({ CSSTokenType::Property, std::string(prop) });
                        tokens.push_back({ CSSTokenType::Value,    std::string(val)  });
                    }
                } else if (!prop.empty()) {
                    i++; // Safety Fallback
                }
            }
        }
    }

    return tokens;
}