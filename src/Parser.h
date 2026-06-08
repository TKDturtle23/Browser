//
// Created by tkdtu on 5/27/2026.
//

#ifndef BROWSER_PARSER_H
#define BROWSER_PARSER_H



#include "Node.h"
#include "Tokenizer.h"
#include "Render/Renderer.h"
void ComputeStyle(Node& node, const Style* parentStyle = nullptr);
class Parser {
    public:
    static Node Parse(const std::vector<Token>& tokens);
    void PrintNode(const Node& node, int depth = 0);
    static Node *FindNodeByTag(Node* dom, const std::string& tag);
};




#endif //BROWSER_PARSER_H
