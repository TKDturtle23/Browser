//
// Created by tkdtu on 5/31/2026.
//

#ifndef BROWSER_WORDCOLLECTOR_H
#define BROWSER_WORDCOLLECTOR_H
#include <functional>

#include "Node.h"
#include "Text/Font.h"

// ===========================================================================
//  WordCollector — walks an inline subtree and emits Words
// ===========================================================================

struct Word {
    Node*       node           = nullptr;
    std::string text;
    int         width          = 0;
    int         height         = 0;
    int         fontSize       = 0;
    bool        hasSpaceBefore = false;
    bool        bold           = false;
    bool        italic         = false;
    bool        isImage        = false;
};

class WordCollector {
public:
    WordCollector(std::shared_ptr<Font> base, std::shared_ptr<Font> italic, std::shared_ptr<Font> bold, std::shared_ptr<Font> boldItalic,
                  std::vector<Word>& out,
                  std::function<Font&(const Style&)> resolveFont, int vw, int vh)
        : base_(base), italic_(italic), bold_(bold), boldItalic_(boldItalic)
        , out_(out)
        , resolveFont_(std::move(resolveFont)), vw(vw), vh(vh)
    {}

    void Visit(Node& node);

private:
    void VisitImage(Node& node);

    void VisitText(Node& node);

    static int MeasureText(Font& font, const std::string& s);
    int vw, vh;
    std::shared_ptr<Font> base_;
    std::shared_ptr<Font> italic_;
    std::shared_ptr<Font> bold_;
    std::shared_ptr<Font> boldItalic_;
    std::vector<Word>& out_;
    std::function<Font&(const Style&)> resolveFont_;
    bool pendingSpace_ = false;
};



#endif //BROWSER_WORDCOLLECTOR_H
