#pragma once

#include <string>
#include "quickjs.h"

// Forward declarations
struct Node;
class QuickjsEngine;

// ============================================================
//  QuickJS inline property/function helpers
//
//  These are thin wrappers around JS_SetPropertyStr /
//  JS_DefinePropertyGetSet to keep call-sites readable.
//  Include this header wherever you need to attach properties
//  or methods to a JSValue without pulling in all of DOMBridge.
// ============================================================

namespace QuickJS {

inline void set_string_prop(JSContext *ctx, JSValue obj,
                            const char *key, const std::string &value) {
    JS_SetPropertyStr(ctx, obj, key, JS_NewString(ctx, value.c_str()));
}

inline void set_func_prop(JSContext *ctx, JSValue obj,
                          const char *name, JSCFunction *func, int argc) {
    JS_SetPropertyStr(ctx, obj, name, JS_NewCFunction(ctx, func, name, argc));
}

// Accessor pair — both getter and setter use the same magic int
inline void set_accessor_prop(JSContext *ctx, JSValue obj, const char *name, int magic,
                              JSCFunctionMagic *getter, JSCFunctionMagic *setter) {
    JSAtom atom = JS_NewAtom(ctx, name);
    JS_DefinePropertyGetSet(ctx, obj, atom,
        JS_NewCFunctionMagic(ctx, getter, "get", 0, JS_CFUNC_generic_magic, magic),
        JS_NewCFunctionMagic(ctx, setter, "set", 1, JS_CFUNC_generic_magic, magic),
        JS_PROP_C_W_E);
    JS_FreeAtom(ctx, atom);
}

// Accessor pair with explicit flags and optional setter (pass nullptr to omit)
inline void set_accessor_prop_magic(JSContext *ctx, JSValue obj, const char *name, int magic,
                                    JSCFunctionMagic *getter, JSCFunctionMagic *setter,
                                    int flags = JS_PROP_C_W_E) {
    JSAtom atom = JS_NewAtom(ctx, name);
    JSValue set_val = setter
        ? JS_NewCFunctionMagic(ctx, setter, "set", 1, JS_CFUNC_generic_magic, magic)
        : JS_UNDEFINED;
    JS_DefinePropertyGetSet(ctx, obj, atom,
        JS_NewCFunctionMagic(ctx, getter, "get", 0, JS_CFUNC_generic_magic, magic),
        set_val, flags);
    JS_FreeAtom(ctx, atom);
}

} // namespace QuickJS

// ============================================================
//  DOM tree utilities
// ============================================================

// Retrieve the QuickjsEngine pointer stored in the global object.
// Every C callback that needs engine state calls this.
QuickjsEngine *GetEngine(JSContext *ctx);

// DFS search for the first <body> node in the tree.
Node *FindBody(Node *root);
Node *FindHead(Node *root);