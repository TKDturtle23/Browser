//
// Created by tkdtu on 5/31/2026.
//

#include "WordCollector.h"
#include "LayoutHelper.h"
#include "Render/Backend/IRendererBackend.h"

void WordCollector::Visit(Node &node) {
    // Walk up the parent chain to find a resolved px font size to use as the
    // em base. Parents may themselves be em/rem/% so we collect the chain
    // and resolve top-down, each step feeding into the next.
    std::vector<const Style*> chain;
    Node* p = node.parent;
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

    Visit(node, resolved);
}

void WordCollector::Visit(Node &node, int inheritedFontSize) {
    if (IsLayoutIgnored(node)) return;

    if (node.type == NodeType::Text) {
        VisitText(node, inheritedFontSize);
        return;
    }

    if (node.type != NodeType::Element) return;

    if (node.tag == "img") {
        VisitImage(node);
        return;
    }

    // Resolve this element's font size using the inherited size as the em base.
    // This is correct per CSS: 'em' on font-size is relative to the *parent's*
    // font size, not the element's own (which isn't resolved yet).
    Font& font = resolveFont_(node.computedStyle);
    int resolvedSize = ResolveFontSize(node.computedStyle.font_size, vw, vh, inheritedFontSize);
    font.SetSize(IRenderBackend::GetRenderBackend().get(), resolvedSize);

    for (const auto& child : node.children) {
        if (child->type == NodeType::Element
            && child->computedStyle.display == DisplayType::Block)
            continue;
        // Pass this element's resolved size as the inherited size for children,
        // so nested em values chain correctly.
        Visit(*child, resolvedSize);
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

void WordCollector::VisitText(Node &node, int inheritedFontSize) {
    const Style& style = (node.parent) ? node.parent->computedStyle : node.computedStyle;
    Font& font = resolveFont_(style);
    // Use inheritedFontSize as the em base. For a text node the "inherited"
    // size is the parent element's already-resolved size, passed in by Visit.
    font.SetSize(
    IRenderBackend::GetRenderBackend().get(),
    inheritedFontSize
);
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
        w += font.GetGlyph(IRenderBackend::GetRenderBackend().get(), c).advance;
        prev = c;
    }

    char last = s.back();
    if (prev) w += font.GetKerning(last, prev).x >> 6;

    const auto& g = font.GetGlyph(IRenderBackend::GetRenderBackend().get(), last);
    w += std::max(g.advance, g.bearingX + g.width);

    return w;
}