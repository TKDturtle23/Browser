//
// Created by tkdtu on 5/29/2026.
//

#ifndef BROWSER_JS_FUNCTIONS_H
#define BROWSER_JS_FUNCTIONS_H
#include <memory>
#include <string>

#include "../Node/Node.h"
#include "quickjs.h"

struct EventListener {
    std::string type;
    JSValue callback;
    Node* target_node;
};
struct JavascriptContext {
    Node *document_node;
    std::string *title;
    int GroupLevel = 0;

    std::unordered_map<std::string, std::vector<EventListener>> windowListeners;
};
enum DOMEventMagic {
    EVENT_ONINPUT = 100,
    EVENT_ONCLICK,
    EVENT_ONCHANGE,
    EVENT_ONKEYDOWN
};

inline std::string magic_to_event_name(int magic) {
    switch (magic) {
        case EVENT_ONINPUT:  return "input";
        case EVENT_ONCLICK:  return "click";
        case EVENT_ONCHANGE: return "change";
        case EVENT_ONKEYDOWN: return "keydown";
        default:             return "";
    }
}
namespace JavascriptFunctions {

    void SetNewContext(JavascriptContext context);

    JSValue js_document_get_element_by_id(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    JSValue js_document_get_elements_by_tag_name(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);


    JSValue js_element_get_property(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

    JSValue js_element_set_property(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

    JSValue js_document_get_data(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
    JSValue js_document_set_data(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

    JSValue js_console_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

    JSValue js_element_script_get_property(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
    JSValue js_element_script_set_property(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

    JSValue js_add_event_listener(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    JSValue js_remove_event_listener(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    JSValue js_document_create_element(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);
    JSValue js_document_appendChild(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    JSValue js_node_insert_before(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    JSValue js_document_createTextNode(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    JSValue js_element_removeChild(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    JSValue js_document_query_selector(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    JSValue js_element_set_attribute(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    JSValue js_return_node_type(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

    JSValue js_element_get_event_property(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic);

    JSValue js_element_set_event_property(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic);

    JSValue js_element_set_style_attribute(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    JSValue js_element_get_style_attribute(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    JSValue js_element_get_owner_document(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic);

    JSValue js_element_get_root_node(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    JSValue js_document_get_active_element(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic);
    JSValue js_document_query_selector_all(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
    bool node_matches_selector(Node* n, const std::string& selector);
};

#endif //BROWSER_JS_FUNCTIONS_H
