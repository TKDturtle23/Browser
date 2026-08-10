//
// Created by tkdtu on 6/15/2026.
//

#include "ConsoleBridge.h"

#include "JavaScriptEngine/JS_Functions.h"
#include "../QuickjsEngine.h"
void ConsoleBridge::initialize(JSContext* ctx, const std::string& url, Node *DOM, QuickjsEngine *engine) {
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue console_obj = JS_NewObject(ctx);

    auto add = [&](const char* name, int magic) {
        JS_SetPropertyStr(ctx, console_obj, name,
            JS_NewCFunctionMagic(ctx, JavascriptFunctions::js_console_log,
                                 name, 1, JS_CFUNC_generic_magic, magic));
    };

    add("log", 0); add("info", 1); add("warn", 2);
    add("error", 3); add("debug", 4);
    add("group", 5); add("groupCollapsed", 5); add("groupEnd", 6);

    JS_SetPropertyStr(ctx, global_obj, "console", console_obj);
    JS_FreeValue(ctx, global_obj);
}

void ConsoleBridge::tick() {
}
