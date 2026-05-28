#include "CSSTokenizer.h"
#include <cctype>

std::vector<CSSToken> CSSTokenizer::Tokenize(const std::string& css) {
    std::vector<CSSToken> tokens;
    size_t i = 0;
    bool inBlock = false; // true = inside { }, false = in selector position

    auto skipWhitespace = [&]() {
        while (i < css.size() && std::isspace((unsigned char)css[i])) i++;
    };

    auto readUntil = [&](const std::string& stopChars) {
        std::string out;
        while (i < css.size() && stopChars.find(css[i]) == std::string::npos)
            out += css[i++];
        // trim trailing whitespace
        size_t end = out.find_last_not_of(" \t\n\r");
        return end == std::string::npos ? std::string{} : out.substr(0, end + 1);
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
                // read selector — stop at { or ,
                std::string sel = readUntil("{,");
                if (!sel.empty())
                    tokens.push_back({ CSSTokenType::Selector, sel });
            }
        } else {
            if (c == '}') {
                tokens.push_back({ CSSTokenType::CloseBrace, "}" });
                inBlock = false;
                i++;
            } else if (c == ';') {
                tokens.push_back({ CSSTokenType::Semicolon, ";" });
                i++;
            } else {
                // read property name
                std::string prop = readUntil(":;}");
                if (i < css.size() && css[i] == ':') {
                    i++; // skip colon
                    skipWhitespace();
                    std::string val = readUntil(";}");
                    if (!prop.empty() && !val.empty()) {
                        tokens.push_back({ CSSTokenType::Property, prop });
                        tokens.push_back({ CSSTokenType::Value,    val  });
                    }
                }
            }
        }
    }

    return tokens;
}