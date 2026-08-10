#pragma once
#include <string>
#include <vector>
#include "../Node/Node.h"

struct CSSDeclaration {
    std::string property;
    std::string value;
};

struct CSSSelector {
    std::string tag;      // "div", "h1", "" = any
    std::string id;       // without #
    std::string cls;      // without .
    // pseudo-class is stripped and ignored for now
};



struct CSSRule {
    std::vector<CSSSelector> selectors; // comma-separated selectors share one block
    std::vector<CSSDeclaration> declarations;
};

class CSSParser {
public:
    std::vector<CSSRule> Parse(const std::string &css, bool isInlineStyle);
    void Apply(const std::vector<CSSRule>& rules, Node& root, int viewportWidth, int viewportHeight);

private:
    static CSSSelector ParseSelector(std::string_view s) ;
    bool Matches(const CSSSelector& sel, const Node& node);
    void ApplyDeclarations(const std::vector<CSSDeclaration>& decls, Node& node,
                       int viewportWidth, int viewportHeight);

    void ApplyProperties(Node &node, int vw, int vh);

    void ApplyToTree(const std::vector<CSSRule> &rules, Node &node, int vw, int vh);
};