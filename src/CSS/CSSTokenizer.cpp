#include "CSSTokenizer.h"
#include <cctype>
#include <algorithm>

// Added isInlineStyle parameter to dynamically bypass brace rules
std::vector<CSSToken> CSSTokenizer::Tokenize(const std::string& css, bool isInlineStyle) {
    std::vector<CSSToken> tokens;
    size_t i = 0;

    // If it's an inline style string, we are already logically "inside a block"
    bool inBlock = isInlineStyle;

    // Robust trimming helper to clear whitespaces safely
    auto cleanString = [](const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return std::string{};
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    };

    auto skipWhitespace = [&]() {
        while (i < css.size() && std::isspace((unsigned char)css[i])) i++;
    };

    auto readUntil = [&](const std::string& stopChars) {
        std::string out;
        while (i < css.size() && stopChars.find(css[i]) == std::string::npos) {
            out += css[i++];
        }
        return cleanString(out);
    };

    while (i < css.size()) {
        skipWhitespace();
        if (i >= css.size()) break;

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
                std::string sel = readUntil("{,");
                if (!sel.empty()) {
                    tokens.push_back({ CSSTokenType::Selector, sel });
                }
            }
        } else {
            // If parsing standard stylesheets, look out for closing braces
            if (!isInlineStyle && c == '}') {
                tokens.push_back({ CSSTokenType::CloseBrace, "}" });
                inBlock = false;
                i++;
            } else if (c == ';') {
                tokens.push_back({ CSSTokenType::Semicolon, ";" });
                i++;
            } else {
                // Read Property Name
                std::string prop = readUntil(":;}");

                skipWhitespace();
                if (i < css.size() && css[i] == ':') {
                    i++; // Skip the colon splitter
                    skipWhitespace();

                    // Read Value
                    std::string val = readUntil(";;}"); // Stops cleanly at semicolon or brace end

                    if (!prop.empty() && !val.empty()) {
                        tokens.push_back({ CSSTokenType::Property, prop });
                        tokens.push_back({ CSSTokenType::Value,    val  });
                    }
                } else if (!prop.empty()) {
                    // Safety Fallback: advance step to prevent infinite loops on malformed CSS syntax
                    i++;
                }
            }
        }
    }

    return tokens;
}