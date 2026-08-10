//
// Created by tkdtu on 6/22/2026.
//

#ifndef BROWSER_DOMSTYLE_H
#define BROWSER_DOMSTYLE_H
#include "quickjs.h"
#include "Node/Node.h"

void SetupStyleObject(JSContext *ctx, JSValue js_el, Node *n);
#endif //BROWSER_DOMSTYLE_H
