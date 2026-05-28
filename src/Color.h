#pragma once

#include <cstdint>

struct Color {

    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;

    Color(
        uint8_t blue = 0,
        uint8_t green = 0,
        uint8_t red = 0,
        uint8_t alpha = 255
    )
        : b(blue),
          g(green),
          r(red),
          a(alpha) {}
};