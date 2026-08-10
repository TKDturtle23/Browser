//
// Created by tkdtu on 6/24/2026.
//

#ifndef BROWSER_TEXTENCODER_H
#define BROWSER_TEXTENCODER_H
#include "quickjs.h"


class TextEncoder {
public:
    static void SetupTextEncoder(JSContext *ctx, JSValue global);
};




#endif //BROWSER_TEXTENCODER_H
