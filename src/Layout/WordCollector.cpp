//
// Created by tkdtu on 5/31/2026.
//

#include "WordCollector.h"
#include "LayoutHelper.h"
void WordCollector::Visit(Node &node) {
    if (IsLayoutIgnored(node)) return;

    if (node.type == NodeType::Text) {
        VisitText(node);
        return;
    }

    if (node.type != NodeType::Element) return;

    if (node.tag == "img") {
        VisitImage(node);
        return;
    }

    Font& font = resolveFont_(node.computedStyle);
    font.SetSize(ResolveFontSize(node.computedStyle.font_size, vw, vh));

    for (const auto& child : node.children) {
        if (child->type == NodeType::Element
            && child->computedStyle.display == DisplayType::Block)
            continue;
        Visit(*child);
    }
}

void WordCollector::VisitImage(Node &node) {
    // Dimension fallback sequence: CSS > attributes > intrinsic > default
    int imgW = 32, imgH = 32;

    if (node.attributes.contains("width"))  imgW = std::stoi(node.attributes.at("width"));
    if (node.attributes.contains("height")) imgH = std::stoi(node.attributes.at("height"));

    if (node.computedStyle.width.unit  == LengthUnit::Px) imgW = static_cast<int>(node.computedStyle.width.value);
    if (node.computedStyle.height.unit == LengthUnit::Px) imgH = static_cast<int>(node.computedStyle.height.value);

    if (node.imageData && node.imageData->isLoaded) {
        if (!node.attributes.contains("width")  && node.computedStyle.width.unit  == LengthUnit::Auto) imgW = node.imageData->intrinsicWidth;
        if (!node.attributes.contains("height") && node.computedStyle.height.unit == LengthUnit::Auto) imgH = node.imageData->intrinsicHeight;
    }

    Word w;
    w.node           = &node;
    w.isImage        = true;
    w.hasSpaceBefore = pendingSpace_;
    w.width          = imgW;
    w.height         = imgH;

    out_.push_back(std::move(w));
    pendingSpace_ = false;
}

void WordCollector::VisitText(Node &node) {
    const Style& style = (node.parent) ? node.parent->computedStyle : node.computedStyle;
    Font& font = resolveFont_(style);
    font.SetSize(ResolveFontSize(style.font_size, vw, vh));

    const std::string& t = node.text;
    size_t i = 0;

    while (i < t.size()) {
        if (std::isspace(static_cast<unsigned char>(t[i]))) {
            pendingSpace_ = true;
            while (i < t.size() && std::isspace(static_cast<unsigned char>(t[i]))) ++i;
            continue;
        }

        size_t start = i;
        while (i < t.size() && !std::isspace(static_cast<unsigned char>(t[i]))) ++i;

        Word w;
        w.node           = &node;
        w.text           = t.substr(start, i - start);
        w.width          = MeasureText(font, w.text);
        w.fontSize       = font.GetCurrentSize();
        w.hasSpaceBefore = pendingSpace_;
        w.bold           = style.font_bold;
        w.italic         = style.font_italic;

        out_.push_back(std::move(w));
        pendingSpace_ = false;
    }
}

int WordCollector::MeasureText(Font &font, const std::string &s) {
    if (s.empty()) return 0;

    int w = 0;
    char prev = 0;

    for (size_t i = 0; i < s.size() - 1; ++i) {
        char c = s[i];
        if (prev) w += font.GetKerning(c, prev).x >> 6;
        w += font.GetGlyph(c).advance;
        prev = c;
    }

    char last = s.back();
    if (prev) w += font.GetKerning(last, prev).x >> 6;

    const auto& g = font.GetGlyph(last);
    w += std::max(g.advance, g.bearingX + g.width);

    return w;
}
