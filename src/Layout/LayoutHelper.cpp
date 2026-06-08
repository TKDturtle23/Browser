//
// Created by tkdtu on 5/31/2026.
//
#include "LayoutHelper.h"

#include <algorithm>
#include <unordered_set>

bool IsBlank(const std::string& s) {
    return std::all_of(s.begin(), s.end(),
        [](char c) { return std::isspace(static_cast<unsigned char>(c)); });
}

double ResolveLength(const CSSLength& len, double referenceSize, int Vw, int Vh, float fontSize) {
    switch (len.unit) {
        case LengthUnit::Percent: return static_cast<int>((len.value / 100.0f) * referenceSize);
        case LengthUnit::Px:      return static_cast<int>(len.value);
            case LengthUnit::Vw:      return static_cast<int>((len.value / 100.0f) * Vw);
            case LengthUnit::Vh:      return static_cast<int>((len.value / 100.0f) * Vh);
case LengthUnit::Em:      return static_cast<int>(len.value * fontSize);
        default:                  return 0;
    }
}

double ResolveFontSize(const CSSLength& fontSize, int vw, int vh, float inheritedFontSize) {
    constexpr int kDefaultFontSize = 16;
    if (fontSize.unit != LengthUnit::Auto)
        return ResolveLength(fontSize, kDefaultFontSize, vw, vh, inheritedFontSize);
    return kDefaultFontSize;
}

bool IsNonRendered(const std::string& tag) {
    static const std::unordered_set<std::string> kTags = {
        "head", "title", "meta", "link", "script", "style", "base", "noscript"
    };
    return kTags.contains(tag);
}

bool IsLayoutIgnored(const Node& n) {
    if (n.type == NodeType::Doctype || n.type == NodeType::Document) return true;
    if (n.type == NodeType::Element && IsNonRendered(n.tag))         return true;
    return false;
}

double GetVisibleBorderWidth(const BorderSide& side, int vw, int vh, float fontSize) {
    if (side.borderWidth.unit == LengthUnit::Auto || side.borderWidth.value < 0.0f)
        return 0;
    return ResolveLength(side.borderWidth, 16, vw, vh, fontSize);
}

bool IsInlineTag(const std::string& tag) {
    static const std::unordered_set<std::string> kInlineTags = {
        "span", "a", "em", "strong", "code", "i", "b", "img", "#text"
    };
    std::string lower = tag;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return kInlineTags.contains(lower);
}

bool IsInlineChild(const Node& n) {
    if (n.type == NodeType::Text)    return true;
    if (n.type != NodeType::Element) return false;
    if (n.computedStyle.display == DisplayType::Block) return false;
    return IsInlineTag(n.tag);
}

double ResolveFontSizeInherit(Node* n, int vw, int vh)
{
    std::vector<const Style*> chain;
    Node* p = n->parent;
    while (p) {
        chain.push_back(&p->computedStyle);
        // Stop once we hit an absolute unit — no need to go further.
        if (p->computedStyle.font_size.unit == LengthUnit::Px /* ||
            p->computedStyle.font_size.unit == LengthUnit::Pt*/) break;
        p = p->parent;
    }

    // Resolve top-down: the topmost ancestor uses the browser default (16px).
    int resolved = 16;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        resolved = ResolveFontSize((*it)->font_size, vw, vh, resolved);
    }
    return ResolveFontSize(n->computedStyle.font_size, vw, vh, resolved);
}
