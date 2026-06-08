//
// Created by tkdtu on 5/31/2026.
//

#ifndef BROWSER_LAYOUTHELPER_H
#define BROWSER_LAYOUTHELPER_H
#include <string>

#include "Node.h"
#include "Text/Font.h"
struct FontGroup {
    std::shared_ptr<Font> base;
    std::shared_ptr<Font> bold;
    std::shared_ptr<Font> italic;
    std::shared_ptr<Font> boldItalic;
};
enum class BoxKind {
    Block,
    Line,
    TextRun,
};
struct BoxEdges {
    int top = 0, right = 0, bottom = 0, left = 0;
    BoxEdges() = default;
    BoxEdges(int top, int right, int bottom, int left) : top(top), right(right), bottom(bottom), left(left) {}
    int Horizontal() const { return left + right; }
    int Vertical() const { return top + bottom; }
};
struct LayoutBox {
    BoxKind kind = BoxKind::Block;

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int fontSize = 0; // for TextRun boxes

    int lineAscent = 0;
    int lineDescent = 0;

    Node* node = nullptr;

    // Only used when kind == TextRun.
    std::string text;

    std::vector<LayoutBox> children;
};
bool IsBlank(const std::string& s);
int ResolveLength(const CSSLength& len, int referenceSize, int Vw, int Vh);
int ResolveFontSize(const CSSLength& fontSize, int vw, int vh, float inheritedFontSize);
bool IsNonRendered(const std::string& tag);
bool IsLayoutIgnored(const Node& n);
int GetVisibleBorderWidth(const BorderSide& side, int vw, int vh);
bool IsInlineTag(const std::string& tag);
bool IsInlineChild(const Node& n);
#endif //BROWSER_LAYOUTHELPER_H
