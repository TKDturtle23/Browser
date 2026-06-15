//
// Created by tkdtu on 6/10/2026.
//

#ifndef BROWSER_FLEX_H
#define BROWSER_FLEX_H
#include "CSSLength.h"
enum class FlexDirection {
    Row,
    RowReverse,
    Column,
    ColumnReverse,
};

enum class FlexWrap {
    NoWrap,
    Wrap,
    WrapReverse,
};

enum class JustifyContent {
    Start,
    End,
    Center,
    FlexStart,
    FlexEnd,
    Left,
    Right,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly,
    Stretch,
};

enum class AlignItems {
    Normal = 1,
    Start,
    End,
    Center,
    FlexStart,
    FlexEnd,
    SelfStart,
    SelfEnd,
    Baseline,
    FirstBaseline,
    LastBaseline,
    Stretch,
};

enum class AlignSelf {
    Auto,
    Normal,
    Start,
    End,
    Center,
    FlexStart,
    FlexEnd,
    SelfStart,
    SelfEnd,
    Baseline,
    FirstBaseline,
    LastBaseline,
    Stretch,
};

enum class AlignContent {
    Normal,
    Start,
    End,
    Center,
    FlexStart,
    FlexEnd,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly,
    Stretch,
    Baseline,
    FirstBaseline,
    LastBaseline,
};

enum class OverflowAlignment {
    Default,
    Safe,
    Unsafe,
};


struct FlexData {

    // =========================================================
    // Container properties
    // =========================================================

    FlexDirection direction = FlexDirection::Row;

    FlexWrap wrap = FlexWrap::NoWrap;

    JustifyContent justifyContent =
        JustifyContent::FlexStart;

    AlignItems alignItems =
        AlignItems::Stretch;

    AlignContent alignContent =
        AlignContent::Stretch;

    OverflowAlignment justifyOverflow =
        OverflowAlignment::Default;

    OverflowAlignment alignOverflow =
        OverflowAlignment::Default;

    // =========================================================
    // Gap support
    // =========================================================

    CSSLength rowGap {
        0.0f,
        LengthUnit::Px
    };

    CSSLength columnGap {
        0.0f,
        LengthUnit::Px
    };

    void SetGap(const CSSLength& gap)
    {
        rowGap = gap;
        columnGap = gap;
    }

    // =========================================================
    // Item properties
    // =========================================================

    int order = 0;

    float grow = 0.0f;

    float shrink = 1.0f;

    CSSLength basis {
        0.0f,
        LengthUnit::Auto
    };

    AlignSelf alignSelf =
        AlignSelf::Auto;

    // =========================================================
    // Optional sizing constraints
    // =========================================================

    CSSLength minWidth {
        0.0f,
        LengthUnit::Auto
    };

    CSSLength minHeight {
        0.0f,
        LengthUnit::Auto
    };

    CSSLength maxWidth {
        0.0f,
        LengthUnit::Auto
    };

    CSSLength maxHeight {
        0.0f,
        LengthUnit::Auto
    };
};
#endif //BROWSER_FLEX_H
