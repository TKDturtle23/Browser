//
// Created by tkdtu on 6/2/2026.
//

#include "FontManager.h"
#include "Layout/LayoutHelper.h"
#include "Render/Renderer.h"

FallbackFonts* FontManager::fallbackFont;
std::unordered_map<std::string, FontGroup> FontManager::Fonts;

Font& FontManager::ResolveFont(const Style& s) {
    std::string targetFamily = "Arial";
    auto it = Fonts.find(targetFamily);

    if (it == Fonts.end() && !Fonts.empty()) {
        it = Fonts.begin();
    }

    if (it != Fonts.end()) {
        FontGroup& group = it->second;

        // Safely dereference the shared pointers here
        if (s.font_bold && s.font_italic) return *group.boldItalic;
        if (s.font_bold)                  return *group.bold;
        if (s.font_italic)                return *group.italic;
        return *group.base;
    }

    return fallbackFont->Primary;
}

FontMetrics FontManager::PrepareFontContext(
    const Style& s,
    int forcedSize,
    Font*& outFont,
    int rendererWidth,
    int rendererHeight
) {
    outFont = &ResolveFont(s);

    int size = (forcedSize > 0)
        ? forcedSize
        : ResolveFontSize(s.font_size, rendererWidth, rendererHeight, 16);

    if (size <= 0)
        size = 16;

    // This now executes smoothly without crashing your cross-platform abstraction!
    outFont->SetSize(IRenderBackend::GetRenderBackend().get(), size);

    return outFont->GetMetrics();
}

void FontManager::AddFont(std::string name, const FontGroup& group) {
    // Because shared_ptr can be cleanly assigned, insert_or_assign now works perfectly!
    Fonts.insert_or_assign(std::move(name), group);
}

FontGroup FontManager::GetFontGroup(std::string name) {
    return Fonts.at(name);
}

void FontManager::setFallbackFont(FallbackFonts *font) {
    fallbackFont = font;
}

FallbackFonts * FontManager::getFallbackFont() {
    return fallbackFont;
}
