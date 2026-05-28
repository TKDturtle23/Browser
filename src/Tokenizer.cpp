//
// Created by tkdtu on 5/26/2026.
//

#include "Tokenizer.h"
#include <algorithm>
std::vector<Token> Tokenizer::tokenize(const std::string& html) {
    std::vector<Token> tokens;

    size_t i = 0;

    while (i < html.length()) {

        // TAGS
        if (html[i] == '<') {

            // Closing tag
            if (i + 1 < html.length() && html[i + 1] == '/') {

                i += 2;

                std::string tag;

                while (i < html.length() &&
                       html[i] != '>') {

                    tag += html[i];
                    i++;
                       }

                tokens.push_back(
                    Token(TokenType::CloseTag, tag, {})
                );

                if (i < html.length())
                    i++;

                continue;
            }

            // Opening tag
            i++;

            std::string tag;

            while (i < html.length() &&
                   html[i] != ' ' &&
                   html[i] != '>') {

                tag += html[i];
                i++;
                   }

            std::unordered_map<std::string, std::string> attributes;

            while (i < html.length() &&
                   html[i] != '>') {

                // skip whitespace
                while (i < html.length() &&
                       std::isspace(html[i])) {
                    i++;
                       }

                if (html[i] == '>' || html[i] == '/')
                    break;

                std::string attrName;
                std::string attrValue;

                // parse attribute name
                while (i < html.length() &&
                       html[i] != '=' &&
                       !std::isspace(html[i]) &&
                       html[i] != '>') {

                    attrName += html[i];
                    i++;
                       }

                // skip whitespace
                while (i < html.length() &&
                       std::isspace(html[i])) {
                    i++;
                       }

                // parse value
                if (i < html.length() &&
                    html[i] == '=') {

                    i++;

                    while (i < html.length() &&
                           std::isspace(html[i])) {
                        i++;
                           }

                    // quoted value
                    if (i < html.length() &&
                        (html[i] == '"' || html[i] == '\'')) {

                        char quote = html[i];
                        i++;

                        while (i < html.length() &&
                               html[i] != quote) {

                            attrValue += html[i];
                            i++;
                               }

                        if (i < html.length())
                            i++;
                        }
                    }

                std::transform(
                    attrName.begin(),
                    attrName.end(),
                    attrName.begin(),
                    ::tolower
                );

                attributes[attrName] = attrValue;
                   }
            std::transform(
                tag.begin(),
                tag.end(),
                tag.begin(),
                ::tolower
            );
            tokens.push_back(
                Token(tag == "!doctype" ? TokenType::Doctype : TokenType::OpenTag, tag, attributes)
            );
            if (html[i - 1] == '/') // self closing tag
            {
                tokens.push_back(
                Token(TokenType::CloseTag, tag, attributes)
            );
            }
            if (i < html.length())
                i++;

            continue;
        }

        // TEXT
        std::string text;

        while (i < html.length() &&
               html[i] != '<') {

            text += html[i];
            i++;
               }

        if (!text.empty() &&
    text.find_first_not_of(" \n\t\r") != std::string::npos) {
            tokens.push_back(
                Token(TokenType::Text, text, {})
            );

        }
    }

    return tokens;
}
