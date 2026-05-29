#include "Tokenizer.h"
#include <algorithm>
#include <cctype>

std::vector<Token> Tokenizer::tokenize(const std::string& html) {
    std::vector<Token> tokens;
    size_t i = 0;

    while (i < html.length()) {

        // TAGS
        if (html[i] == '<') {

            // Closing tag (e.g., </div>)
            if (i + 1 < html.length() && html[i + 1] == '/') {
                i += 2;
                std::string tag;
                while (i < html.length() && html[i] != '>') {
                    tag += html[i];
                    i++;
                }
                std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
                tokens.push_back(Token(TokenType::CloseTag, tag, {}));

                if (i < html.length()) i++;
                continue;
            }

            // Opening or Self-Closing tag
            i++;
            std::string tag;
            while (i < html.length() && html[i] != ' ' && html[i] != '/' && html[i] != '>') {
                tag += html[i];
                i++;
            }
            std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
            std::unordered_map<std::string, std::string> attributes;
            bool isSelfClosing = false;

            while (i < html.length() && html[i] != '>') {
                // skip whitespace
                while (i < html.length() && std::isspace(html[i])) {
                    i++;
                }

                // Check for trailing slash indicating self-closing tag (e.g., />)
                if (i < html.length() && html[i] == '/') {
                    isSelfClosing = true;
                    i++;
                    // skip any trailing spaces between / and >
                    while (i < html.length() && std::isspace(html[i])) {
                        i++;
                    }
                    break;
                }

                if (html[i] == '>')
                    break;

                std::string attrName;
                std::string attrValue;

                // parse attribute name
                while (i < html.length() && html[i] != '=' && !std::isspace(html[i]) && html[i] != '>' && html[i] != '/') {
                    attrName += html[i];
                    i++;
                }

                // skip whitespace
                while (i < html.length() && std::isspace(html[i])) {
                    i++;
                }

                // parse value
                if (i < html.length() && html[i] == '=') {
                    i++;
                    while (i < html.length() && std::isspace(html[i])) {
                        i++;
                    }

                    // quoted value
                    if (i < html.length() && (html[i] == '"' || html[i] == '\'')) {
                        char quote = html[i];
                        i++;
                        while (i < html.length() && html[i] != quote) {
                            attrValue += html[i];
                            i++;
                        }
                        if (i < html.length()) i++;
                    }
                }

                if (!attrName.empty()) {
                    std::transform(attrName.begin(), attrName.end(), attrName.begin(), ::tolower);
                    attributes[attrName] = attrValue;
                }
            }

            // Emit the opening tag
            tokens.push_back(
                Token(tag == "!doctype" ? TokenType::Doctype : TokenType::OpenTag, tag, attributes)
            );

            // Emit the closing tag immediately if it's self-closing
            if (isSelfClosing) {
                tokens.push_back(Token(TokenType::CloseTag, tag, {}));
            }

            if (i < html.length() && html[i] == '>') i++;

            // ─── CRITICAL FIX: SCRIPT & STYLE BYPASS ───
            // Only parse internal content if it wasn't a self-closed script/style (e.g. <script src="..." />)
            if (!isSelfClosing && (tag == "script" || tag == "style")) {
                std::string rawContent;
                std::string closeTarget = "</" + tag + ">";
                size_t startPos = i;
                
                // Search for case-insensitive closing block sequence
                size_t endPos = std::string::npos;
                for (size_t j = startPos; j + closeTarget.length() <= html.length(); ++j) {
                    std::string segment = html.substr(j, closeTarget.length());
                    std::transform(segment.begin(), segment.end(), segment.begin(), ::tolower);
                    if (segment == closeTarget) {
                        endPos = j;
                        break;
                    }
                }

                if (endPos != std::string::npos) {
                    rawContent = html.substr(startPos, endPos - startPos);
                    i = endPos; // Skip the pointer directly to the closing tag position
                } else {
                    // Malformed file handling (script tag never closes)
                    rawContent = html.substr(startPos);
                    i = html.length();
                }

                if (!rawContent.empty()) {
                    tokens.push_back(Token(TokenType::Text, rawContent, {}));
                }
            }
            // ───────────────────────────────────────────

            continue;
        }

        // TEXT
        std::string text;
        while (i < html.length() && html[i] != '<') {
            text += html[i];
            i++;
        }

        if (!text.empty() && text.find_first_not_of(" \n\t\r") != std::string::npos) {
            tokens.push_back(Token(TokenType::Text, text, {}));
        }
    }

    return tokens;
}