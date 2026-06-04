//
// Created by tkdtu on 6/2/2026.
//
#include "Text/Font.h"
#include "Node.h"
#include "Layout/LayoutHelper.h"
#ifndef BROWSER_FONTMANAGER_H
#define BROWSER_FONTMANAGER_H


class FontManager
{
public:
    static Font& ResolveFont(const Style& s);
    static FontMetrics PrepareFontContext(const Style& s, int forcedSize, Font*& outFont, int rendererWidth, int rendererHeight);
    static void AddFont(std::string name, const FontGroup& group);
    static FontGroup GetFontGroup(std::string name);

    static void setFallbackFont(Font* font);

private:
    static Font* fallbackFont;
    static std::unordered_map<std::string, FontGroup> Fonts;
};


#endif //BROWSER_FONTMANAGER_H
