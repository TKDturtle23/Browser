#include "Tokenizer.h"
#include <algorithm>
#include <cctype>

std::vector<Token> Tokenizer::tokenize(const std::string& html) {
    std::vector<Token> tokens;
    size_t i = 0;

    while (i < html.length()) {

        // TAGS, COMMENTS, & DOCTYPES
        if (html[i] == '<') {

            // ─── 1. HANDLING HTML COMMENTS () ───
            if (i + 3 < html.length() && html[i + 1] == '!' && html[i + 2] == '-' && html[i + 3] == '-') {
                size_t commentStart = i + 4;
                size_t commentEnd = html.find("-->", commentStart);

                if (commentEnd != std::string::npos) {
                    std::string commentText = html.substr(commentStart, commentEnd - commentStart);
                    tokens.push_back(Token(TokenType::Comment, commentText, {}));
                    i = commentEnd + 3; // Advance past "-->"
                } else {
                    std::string commentText = html.substr(commentStart);
                    tokens.push_back(Token(TokenType::Comment, commentText, {}));
                    i = html.length();
                }
                continue;
            }

            // ─── 2. HANDLING DOCTYPE (<!DOCTYPE html>) ───
            if (i + 8 < html.length() && html[i + 1] == '!') {
                std::string lookahead = html.substr(i + 1, 8);
                std::transform(lookahead.begin(), lookahead.end(), lookahead.begin(), ::tolower);

                if (lookahead == "!doctype") {
                    i += 9; // Advance past "<!doctype"

                    // Skip any whitespace after !doctype
                    while (i < html.length() && std::isspace(html[i])) {
                        i++;
                    }

                    std::string doctypeContent;
                    while (i < html.length() && html[i] != '>') {
                        doctypeContent += html[i];
                        i++;
                    }

                    // Trim trailing whitespace from the content (e.g., "html ")
                    while (!doctypeContent.empty() && std::isspace(doctypeContent.back())) {
                        doctypeContent.pop_back();
                    }
                    std::transform(doctypeContent.begin(), doctypeContent.end(), doctypeContent.begin(), ::tolower);

                    // Emit clean Doctype token: Tag name becomes "html", attributes are empty
                    tokens.push_back(Token(TokenType::Doctype, doctypeContent, {}));

                    if (i < html.length() && html[i] == '>') i++;
                    continue;
                }
            }

            // ─── 3. HANDLING CLOSING TAGS (e.g., </div>) ───
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

            // ─── 4. HANDLING OPENING OR SELF-CLOSING TAGS ───
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
                while (i < html.length() && std::isspace(html[i])) {
                    i++;
                }

                if (i < html.length() && html[i] == '/') {
                    isSelfClosing = true;
                    i++;
                    while (i < html.length() && std::isspace(html[i])) {
                        i++;
                    }
                    break;
                }

                if (html[i] == '>')
                    break;

                std::string attrName;
                std::string attrValue;

                while (i < html.length() && html[i] != '=' && !std::isspace(html[i]) && html[i] != '>' && html[i] != '/') {
                    attrName += html[i];
                    i++;
                }

                while (i < html.length() && std::isspace(html[i])) {
                    i++;
                }

                if (i < html.length() && html[i] == '=') {
                    i++;
                    while (i < html.length() && std::isspace(html[i])) {
                        i++;
                    }

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

            // Emit the opening tag (We no longer need the ternary fallback here!)
            tokens.push_back(Token(TokenType::OpenTag, tag, attributes));

            if (isSelfClosing) {
                tokens.push_back(Token(TokenType::CloseTag, tag, {}));
            }

            if (i < html.length() && html[i] == '>') i++;

            // ─── SCRIPT & STYLE BYPASS ───
            if (!isSelfClosing && (tag == "script" || tag == "style")) {
                std::string rawContent;
                std::string closeTarget = "</" + tag + ">";
                size_t startPos = i;

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
                    i = endPos;
                } else {
                    rawContent = html.substr(startPos);
                    i = html.length();
                }

                if (!rawContent.empty()) {
                    tokens.push_back(Token(TokenType::Text, rawContent, {}));
                }
            }

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