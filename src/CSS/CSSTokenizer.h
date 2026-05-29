#pragma once
#include <string>
#include <vector>
#include <unordered_map>

enum class CSSTokenType {
    Selector,
    OpenBrace,    // {
    CloseBrace,   // }
    Property,     // left side of :
    Value,        // right side of :
    Semicolon,    // ;
    Colon,        // :
    Comma,        // ,
};

struct CSSToken {
    CSSTokenType type;
    std::string value;
};

class CSSTokenizer {
public:
    std::vector<CSSToken> Tokenize(const std::string &css, bool isInlineStyle);
};