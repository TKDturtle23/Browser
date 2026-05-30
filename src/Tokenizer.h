//
// Created by tkdtu on 5/26/2026.
//

#ifndef BROWSER_TOKENIZER_H
#define BROWSER_TOKENIZER_H
#include <string>
#include <vector>
#include <unordered_map>
enum class TokenType {
    OpenTag,
    CloseTag,
    Text,
    Doctype,
    Comment,
};

struct Token {
    TokenType type;
    std::string value;
    std::unordered_map<std::string, std::string> attributes;
};
class Tokenizer {
public:
    static std::vector<Token> tokenize(const std::string& html);
};


#endif //BROWSER_TOKENIZER_H
