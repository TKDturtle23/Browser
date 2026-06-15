//
// Created by tkdtu on 6/10/2026.
//

#ifndef BROWSER_CSSLENGTH_H
#define BROWSER_CSSLENGTH_H
enum class LengthUnit {
    Px,
    Percent,
    Auto,
    Em,
    Vw,
    Vh,
    Inherit,
    Content, // flex
};

struct CSSLength {
    float value = 0.0f;
    LengthUnit unit = LengthUnit::Px;
};
#endif //BROWSER_CSSLENGTH_H
