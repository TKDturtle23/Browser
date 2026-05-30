//
// Created by tkdtu on 5/29/2026.
//

#include "NodeToHTML.h"



void AddHTML(const std::unique_ptr<Node> &node, std::string &HTML) {
    if (node->type == NodeType::Text) {
        HTML += node->text;
        return;
    }
HTML += "<" + node->tag;
    for (const auto &a : node->attributes) {
        HTML += " " + a.first + "=\"" + a.second + "\"";
    }
    HTML += ">";

    for (const auto &c : node->children) {
        AddHTML(c, HTML);
    }
    HTML += "</" + node->tag + ">";
}
std::string NodeToHTML::GetHTML(const Node *node) {

    std::string HTML;

    for (const auto &c : node->children) {
        AddHTML(c, HTML);
    }
    return HTML;
}
