//
// Created by tkdtu on 5/29/2026.
//

#ifndef BROWSER_JS_FUNCTIONS_H
#define BROWSER_JS_FUNCTIONS_H
#include <memory>
#include <string>

#include "Node.h"
#include "quickjs.h"
struct JavascriptContext {
    Node *document_node;
    std::string *title;
    int GroupLevel = 0;
};
class JavascriptFunctions {
public:
    static void SetNewContext(JavascriptContext context);
    // 1. Internal C++ Trampoline for document.getElementById
    static JSValue js_document_get_element_by_id(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    // 2. Internal C++ Getters/Setters for HTML Element Properties
    static JSValue js_element_get_property(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

    static JSValue js_element_set_property(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

    static JSValue js_document_get_data(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
    static JSValue js_document_set_data(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

    // log, error, etc
    static JSValue js_console_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);



private:

};

#endif //BROWSER_JS_FUNCTIONS_H
