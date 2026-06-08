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
    double top = 0, right = 0, bottom = 0, left = 0;
    BoxEdges() = default;
    BoxEdges(double top, double right, double bottom, double left) : top(top), right(right), bottom(bottom), left(left) {}
    double Horizontal() const { return left + right; }
    double Vertical() const { return top + bottom; }
};
struct text_char
{
    char c;
    bool highlighted = false;
};
struct Text
{
std::vector<text_char> chars;
    Text() = default;
    Text(std::string t)
    {
        for (auto &c : t)
        {
            chars.push_back({c, false});
        }
    }
};
struct LayoutBox {
    BoxKind kind = BoxKind::Block;

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    double fontSize = 0; // for TextRun boxes

    int lineAscent = 0;
    int lineDescent = 0;

    Node* node = nullptr;

    // Only used when kind == TextRun.
    Text text;

    std::vector<LayoutBox> children;
};
bool IsBlank(const std::string& s);
double ResolveLength(const CSSLength& len, double referenceSize, int Vw, int Vh, float fontSize);
double ResolveFontSize(const CSSLength& fontSize, int vw, int vh, float inheritedFontSize);
bool IsNonRendered(const std::string& tag);
bool IsLayoutIgnored(const Node& n);
double GetVisibleBorderWidth(const BorderSide& side, int vw, int vh, float fontSize);
bool IsInlineTag(const std::string& tag);
bool IsInlineChild(const Node& n);

double ResolveFontSizeInherit(Node* n, int vw, int vh);
#endif //BROWSER_LAYOUTHELPER_H
