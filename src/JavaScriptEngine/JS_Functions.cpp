//
// Created by tkdtu on 5/29/2026.
//

#include "JS_Functions.h"

#include <iostream>
#include <regex>
#include <sstream>
#include <string>

#include "../Debug/Logger.h"
#include "quickjs.h"
#include "QuickjsEngine.h"

#include "Parser.h"
#include "Tokenizer.h"
#include "NodeToHTML.h"
JavascriptContext js_ctx;

// ─── Internal helpers ────────────────────────────────────────────────────────

// FIX: Centralise the repeated engine-pointer retrieval pattern.
static QuickjsEngine* get_engine(JSContext* ctx) {
    JSValue g   = JS_GetGlobalObject(ctx);
    JSValue ptr = JS_GetPropertyStr(ctx, g, "__engine_internal_ptr");
    int64_t addr = 0;
    JS_ToBigInt64(ctx, &addr, ptr);
    JS_FreeValue(ctx, ptr);
    JS_FreeValue(ctx, g);
    return reinterpret_cast<QuickjsEngine*>(static_cast<uintptr_t>(addr));
}

Node* Find_Element_By_ID(std::string ID, Node* DOM) {
    if (DOM->id == ID) {
        return DOM;
    }
    for (auto& child : DOM->children) {
        auto found = Find_Element_By_ID(ID, child.get());
        if (found) {
            return found;
        }
    }
    return nullptr;
}
Node* Find_Element_By_Class(std::string Class, Node* DOM) {
    if (DOM->class_name == Class) {
        return DOM;
    }
    for (auto& child : DOM->children) {
        auto found = Find_Element_By_Class(Class, child.get());
        if (found) {
            return found;
        }
    }
    return nullptr;
}
void Find_Elements_By_Tag(const std::string& tag, Node* node, std::vector<Node*>& results) {
    if (!node) return;

    std::string node_tag = node->tag;
    std::transform(node_tag.begin(), node_tag.end(), node_tag.begin(), ::tolower);

    if (tag == "*" || node_tag == tag) {
        results.push_back(node);
    }

    for (auto &child : node->children) {
        Find_Elements_By_Tag(tag, child.get(), results);
    }
}
Node* Find_Body(Node* DOM) {
    if (DOM->tag == "body") {
        return DOM;
    }
    for (auto& child : DOM->children) {
        auto found = Find_Body(child.get());
        if (found) {
            return found;
        }
    }
    return nullptr;
}
void JavascriptFunctions::SetNewContext(JavascriptContext context) {
    js_ctx = context;
}

// ─── document.getElementById ─────────────────────────────────────────────────

JSValue JavascriptFunctions::js_document_get_element_by_id(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_NULL;
    }

    const char* id_c_str = JS_ToCString(ctx, argv[0]);
    std::string search_id = id_c_str ? id_c_str : "";
    JS_FreeCString(ctx, id_c_str);

    std::cout << "[DOM Bridge] JavaScript requested element ID: " << search_id << std::endl;

    if (!js_ctx.document_node) {
        return JS_NULL;
    }
    Node* mock_element = Find_Element_By_ID(search_id, js_ctx.document_node);
    if (!mock_element) {
        return JS_NULL;
    }

    // FIX: Use centralised helper instead of repeating the pattern inline.
    auto* engine = get_engine(ctx);
    if (!engine) return JS_NULL;
    return engine->wrap_html_element(mock_element);
}

// ─── document.getElementsByTagName ───────────────────────────────────────────

JSValue JavascriptFunctions::js_document_get_elements_by_tag_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_NewArray(ctx);
    }

    const char* tag_c_str = JS_ToCString(ctx, argv[0]);
    std::string search_tag = tag_c_str ? tag_c_str : "";
    JS_FreeCString(ctx, tag_c_str);

    std::transform(search_tag.begin(), search_tag.end(), search_tag.begin(), ::tolower);

    std::cout << "[DOM Bridge] JavaScript requested elements by tag: " << search_tag << std::endl;

    if (!js_ctx.document_node) {
        return JS_NewArray(ctx);
    }

    std::vector<Node*> matches;
    Find_Elements_By_Tag(search_tag, js_ctx.document_node, matches);

    if (matches.empty()) {
        return JS_NewArray(ctx);
    }

    // FIX: Use centralised helper.
    auto* engine = get_engine(ctx);
    if (!engine) return JS_NewArray(ctx);

    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < matches.size(); i++) {
        JSValue wrapped = engine->wrap_html_element(matches[i]);
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, wrapped);
    }

    return arr;
}

// ─── Element property getters ─────────────────────────────────────────────────

JSValue JavascriptFunctions::js_element_get_property(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!element) return JS_UNDEFINED;

    switch (magic) {
        case 0: { // tagName
            std::string uppercase_tag = element->tag;
            std::transform(uppercase_tag.begin(), uppercase_tag.end(), uppercase_tag.begin(), ::toupper);
            return JS_NewString(ctx, uppercase_tag.c_str());
        }
        case 1: return JS_NewString(ctx, element->id.c_str());
        case 2: return JS_NewString(ctx, element->class_name.c_str());
        case 3: return JS_NewString(ctx, NodeToHTML::GetHTML(element).c_str());
        case 4: return JS_NewString(ctx, element->text.c_str());
        default: return JS_UNDEFINED;
    }
}

// ─── Element property setters ─────────────────────────────────────────────────

JSValue JavascriptFunctions::js_element_set_property(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!element || argc < 1) return JS_EXCEPTION;

    const char* c_str = JS_ToCString(ctx, argv[0]);
    std::string value_str = c_str ? c_str : "";
    JS_FreeCString(ctx, c_str);
    Logger::Log("Setting property: %d", "js_element_set_property", 0, magic);

    switch (magic) {
        case 1: element->id = value_str; break;
        case 2: element->class_name = value_str; break;
        case 3: {
            Tokenizer tokenizer;
            auto tokens = tokenizer.tokenize(value_str);
            Parser parser;
            auto sec = parser.ParseFragment(tokens);
            element->children = std::move(sec);
            element->reconstruct = true;
            std::cout << "[DOM Engine] Mutation Detected! New innerHTML: " << value_str << std::endl;
            break;
        }
        case 4: {
            try {
                bool foundTextNode = false;

                for (const auto &child : element->children) {
                    if (child->type == NodeType::Text) {
                        child->text = value_str;
                        child->reconstruct = true;
                        foundTextNode = true;
                        break;
                    }
                }

                if (!foundTextNode) {
                    auto* textNode = new Node();
                    textNode->type = NodeType::Text;
                    textNode->text = value_str;
                    textNode->parent = element;
                    textNode->reconstruct = true;

                    element->children.push_back(std::unique_ptr<Node>(textNode));
                }

                element->reconstruct = true;
            } catch (std::exception& e) {
                Logger::Log(e.what(), "js_element_set_property", 0, magic);
            }

            break;
        }
    }
    return JS_UNDEFINED;
}

// ─── document getters ─────────────────────────────────────────────────────────

JSValue JavascriptFunctions::js_document_get_data(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic) {
    switch (magic) {
        case 0: {
            return js_ctx.title ? JS_NewString(ctx, js_ctx.title->c_str()) : JS_UNDEFINED;
        }
        case 1: {
            auto body = Find_Body(js_ctx.document_node);
            return body ? JS_NewString(ctx, NodeToHTML::GetHTML(body).c_str()) : JS_UNDEFINED;
        }
        default: return JS_UNDEFINED;
    }
}

// ─── document setters ─────────────────────────────────────────────────────────

JSValue JavascriptFunctions::js_document_set_data(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_EXCEPTION;
    }

    switch (magic) {
        case 0: { // title
            const char* c_str = JS_ToCString(ctx, argv[0]);
            std::string value_str = c_str ? c_str : "";
            JS_FreeCString(ctx, c_str);
            // FIX: delete previous allocation before replacing, to avoid leaking the old string.
            delete js_ctx.title;
            js_ctx.title = new std::string(value_str);
            return JS_UNDEFINED; // FIX: explicit return so we don't fall through.
        }
        case 1: { // document.body
            const char* c_str = JS_ToCString(ctx, argv[0]);
            std::string value_str = c_str ? c_str : "";
            JS_FreeCString(ctx, c_str);
            auto body = Find_Body(js_ctx.document_node);
            if (body) {
                body->children.clear();
                Tokenizer tokenizer;
                auto tokens = tokenizer.tokenize(value_str);
                Parser parser;
                auto sec = parser.Parse(tokens);
                body->children = std::move(sec.children);
                body->reconstruct = true;
                std::cout << "[DOM Engine] Mutation Detected! New innerHTML: " << value_str << std::endl;
            }
            return JS_UNDEFINED; // FIX: explicit return, was missing a break before.
        }
    }

    return JS_UNDEFINED;
}

// ─── console ──────────────────────────────────────────────────────────────────

JSValue JavascriptFunctions::js_console_log(JSContext *ctx, JSValue this_val,
                                            int argc, JSValue *argv, int magic) {
    std::string msg;

    for (int i = 0; i < argc; i++) {
        if (i > 0)
            msg += " ";

        const char* c_str = JS_ToCString(ctx, argv[i]);

        if (c_str) {
            msg += c_str;
            JS_FreeCString(ctx, c_str);
        } else {
            JSValue str = JS_ToString(ctx, argv[i]);

            if (!JS_IsException(str)) {
                const char* fallback = JS_ToCString(ctx, str);

                if (fallback) {
                    msg += fallback;
                    JS_FreeCString(ctx, fallback);
                } else {
                    msg += "[object]";
                }
            } else {
                msg += "[exception]";
            }

            JS_FreeValue(ctx, str);
        }
    }

    switch (magic) {
        case 0:
            Logger::Log(msg.c_str(), GetJSFileAndLine(ctx), js_ctx.GroupLevel);
            break;
        case 1:
            Logger::Log_Info(msg.c_str(), GetJSFileAndLine(ctx), js_ctx.GroupLevel);
            break;
        case 2:
            Logger::Log_Warning(msg.c_str(), GetJSFileAndLine(ctx), js_ctx.GroupLevel);
            break;
        case 3:
            Logger::Log_Error(msg.c_str(), GetJSFileAndLine(ctx), js_ctx.GroupLevel);
            break;
        case 4:
            Logger::Log_Debug(msg.c_str(), GetJSFileAndLine(ctx), js_ctx.GroupLevel);
            break;
        case 5: // group
            if (!msg.empty())
                Logger::Log(msg.c_str(), GetJSFileAndLine(ctx), js_ctx.GroupLevel);
            js_ctx.GroupLevel++;
            break;
        case 6: // groupEnd
            if (js_ctx.GroupLevel > 0)
                js_ctx.GroupLevel--;
            break;
        default:
            Logger::Log(msg.c_str(), GetJSFileAndLine(ctx), js_ctx.GroupLevel);
            break;
    }

    return JS_UNDEFINED;
}

// ─── Script element getters/setters ───────────────────────────────────────────

JSValue JavascriptFunctions::js_element_script_get_property(JSContext *ctx, JSValue this_val, int argc, JSValue *argv,
    int magic) {
    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!element) return JS_UNDEFINED;

    switch (magic) {
        // FIX: case 0 (src) documented as not-yet-implemented rather than silently falling through.
        case 0: return JS_UNDEFINED; // TODO: src getter not yet implemented
        case 1: return JS_NewString(ctx, element->code.c_str());
        default: return JS_UNDEFINED;
    }
}

JSValue JavascriptFunctions::js_element_script_set_property(JSContext *ctx, JSValue this_val, int argc, JSValue *argv,
    int magic) {
    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!element || argc < 1) return JS_EXCEPTION;

    const char* c_str = JS_ToCString(ctx, argv[0]);
    std::string value_str = c_str ? c_str : "";
    JS_FreeCString(ctx, c_str);

    switch (magic) {
        case 1: element->code = value_str; break;
    }
    return JS_UNDEFINED;
}

// ─── addEventListener / removeEventListener ───────────────────────────────────

JSValue JavascriptFunctions::js_add_event_listener(
    JSContext* ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst* argv)
{
    if (argc < 2)
        return JS_UNDEFINED;
    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));

    const char* type = JS_ToCString(ctx, argv[0]);

    js_ctx.windowListeners[type].push_back({
        type,
        JS_DupValue(ctx, argv[1]),
        element
    });

    JS_FreeCString(ctx, type);

    return JS_UNDEFINED;
}

JSValue JavascriptFunctions::js_remove_event_listener(
    JSContext* ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst* argv)
{
    if (argc < 2)
        return JS_UNDEFINED;

    const char* type = JS_ToCString(ctx, argv[0]);

    auto it = js_ctx.windowListeners.find(type);
    if (it != js_ctx.windowListeners.end())
    {
        auto& vec = it->second;

        // FIX: JS_FreeValue the callback before erasing, to avoid a QuickJS ref leak.
        for (auto& listener : vec) {
            if (JS_VALUE_GET_PTR(listener.callback) == JS_VALUE_GET_PTR(argv[1])) {
                JS_FreeValue(ctx, listener.callback);
            }
        }

        vec.erase(
            std::remove_if(
                vec.begin(),
                vec.end(),
                [&](const EventListener& l) {
                    return JS_VALUE_GET_PTR(l.callback) == JS_VALUE_GET_PTR(argv[1]);
                }),
            vec.end());
    }

    JS_FreeCString(ctx, type);

    return JS_UNDEFINED;
}

// ─── document.createElement ───────────────────────────────────────────────────

JSValue JavascriptFunctions::js_document_create_element(
    JSContext* ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst* argv
) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_NULL;
    }

    const char* tag_c_str = JS_ToCString(ctx, argv[0]);
    std::string tag = tag_c_str ? tag_c_str : "div";
    JS_FreeCString(ctx, tag_c_str);

    std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);

    std::cout << "[DOM Bridge] createElement: " << tag << std::endl;

    // FIX: Use centralised helper.
    auto* engine = get_engine(ctx);
    if (!engine) return JS_NULL;

    // NOTE: Ownership contract — this raw Node* is unowned until it is adopted
    // into the tree via appendChild/insertBefore. If JS discards it without
    // appending, it will leak. A detached-node pool on the engine would fix this.
    Node* node = new Node();
    node->tag = tag;
    node->children.clear();
    node->attributes.clear();
    node->type = NodeType::Element;

    return engine->wrap_html_element(node);
}

// ─── appendChild ──────────────────────────────────────────────────────────────

JSValue JavascriptFunctions::js_document_appendChild(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
    if (argc < 1) return JS_EXCEPTION;

    auto* parent = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!parent) return JS_EXCEPTION;

    auto* child = static_cast<Node*>(JS_GetOpaque(argv[0], QuickjsEngine::get_node_class_id()));
    if (!child) return JS_EXCEPTION;

    // If already in a tree, unlink from old parent first.
    // NOTE: We release() from the old unique_ptr so there is exactly one owner
    // at all times — the new parent's children vector.
    if (child->parent) {
        auto& old_siblings = child->parent->children;
        auto it = std::find_if(old_siblings.begin(), old_siblings.end(),
            [child](const std::unique_ptr<Node>& n) { return n.get() == child; });
        if (it != old_siblings.end()) {
            it->release(); // relinquish ownership before erasing the slot
            old_siblings.erase(it);
        }
    }

    child->parent = parent;
    parent->children.push_back(std::unique_ptr<Node>(child));
    parent->reconstruct = true;

    Logger::Log("[DOM Engine] Appended child to target node", GetJSFileAndLine(ctx), js_ctx.GroupLevel);

    return JS_DupValue(ctx, argv[0]);
}

// ─── insertBefore ─────────────────────────────────────────────────────────────

JSValue JavascriptFunctions::js_node_insert_before(
    JSContext* ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst* argv
) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "insertBefore requires at least 1 argument");
    }

    auto* parent = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!parent) return JS_EXCEPTION;

    auto* new_child = static_cast<Node*>(JS_GetOpaque(argv[0], QuickjsEngine::get_node_class_id()));
    if (!new_child) return JS_EXCEPTION;

    Node* ref_child = nullptr;
    if (argc >= 2 && !JS_IsNull(argv[1]) && !JS_IsUndefined(argv[1])) {
        ref_child = static_cast<Node*>(JS_GetOpaque(argv[1], QuickjsEngine::get_node_class_id()));
    }

    // FIX: release() from old owner before adopting into new parent — same fix as appendChild.
    if (new_child->parent) {
        auto& old_siblings = new_child->parent->children;
        auto it = std::find_if(old_siblings.begin(), old_siblings.end(),
            [new_child](const std::unique_ptr<Node>& n) { return n.get() == new_child; });
        if (it != old_siblings.end()) {
            it->release();
            old_siblings.erase(it);
        }
    }

    new_child->parent = parent;

    auto& children = parent->children;
    if (!ref_child) {
        children.push_back(std::unique_ptr<Node>(new_child));
    } else {
        auto it = std::find_if(children.begin(), children.end(),
            [ref_child](const std::unique_ptr<Node>& n) { return n.get() == ref_child; });

        if (it != children.end()) {
            children.insert(it, std::unique_ptr<Node>(new_child));
        } else {
            return JS_Throw(ctx, JS_NewString(ctx, "The node before which the new node is to be inserted is not a child of this node."));
        }
    }

    parent->reconstruct = true;
    Logger::Log("[DOM Engine] Inserted child before reference node", GetJSFileAndLine(ctx), js_ctx.GroupLevel);

    return JS_DupValue(ctx, argv[0]);
}

// ─── document.createTextNode ──────────────────────────────────────────────────

JSValue JavascriptFunctions::js_document_createTextNode(
    JSContext* ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst* argv
) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_NULL;
    }

    const char* text_c_str = JS_ToCString(ctx, argv[0]);
    std::string text_val = text_c_str ? text_c_str : "";
    JS_FreeCString(ctx, text_c_str);

    // FIX: Use centralised helper.
    auto* engine = get_engine(ctx);
    if (!engine) return JS_NULL;

    // NOTE: Same detached-node leak caveat as createElement.
    Node* node = new Node();
    node->type = NodeType::Text;
    node->tag = "#text";
    node->text = text_val;
    node->children.clear();
    node->attributes.clear();

    std::cout << "[DOM Engine] Created text node: " << node->text << std::endl;

    return engine->wrap_html_element(node);
}

// ─── removeChild ──────────────────────────────────────────────────────────────

JSValue JavascriptFunctions::js_element_removeChild(
    JSContext* ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "removeChild requires at least 1 argument");
    }

    auto* parent = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!parent) return JS_EXCEPTION;

    auto* child = static_cast<Node*>(JS_GetOpaque(argv[0], QuickjsEngine::get_node_class_id()));
    if (!child) return JS_EXCEPTION;

    auto& children = parent->children;

    auto it = std::find_if(children.begin(), children.end(),
        [child](const std::unique_ptr<Node>& n) { return n.get() == child; });

    if (it == children.end()) {
        // FIX: spec requires a NotFoundError if the child isn't actually ours.
        return JS_ThrowInternalError(ctx, "removeChild: node is not a child of this element");
    }

    // FIX: release() before erasing so the Node* survives for JS to hold a reference to.
    // The JS wrapper (argv[0]) may still be live, so we must not delete the C++ node here.
    it->release();
    children.erase(it);
    child->parent = nullptr;
    parent->reconstruct = true;

    Logger::Log("[DOM Engine] Removed child from target node", GetJSFileAndLine(ctx), js_ctx.GroupLevel);

    // FIX: spec says removeChild returns the removed child, not undefined.
    return JS_DupValue(ctx, argv[0]);
}

// ─── document.querySelector ───────────────────────────────────────────────────
struct AttributeSelector {
    std::string tag_name;     // e.g., "meta"
    std::string attribute;    // e.g., "property"
    std::string value;        // e.g., "csp-nonce"
    bool has_value = false;   // true if checking [attr=val], false if just [attr]
};
bool parse_attribute_selector(const std::string& selector, AttributeSelector& out) {
    // Regex matches: tag_name[attribute="value"] or tag_name[attribute=value] or tag_name[attribute]
    // Captures: 1: tag (optional), 2: attr name, 3: optional quotes/value, 4: raw value
    std::regex attr_regex(R"(^([a-zA-Z0-9_-]*)\s*\[\s*([a-zA-Z0-9_-]+)\s*(?:=\s*(?:['"]([^'"]*)['"]|([^\]'"\s]+)))?\s*\]$)");
    std::smatch match;

    if (std::regex_match(selector, match, attr_regex)) {
        out.tag_name = match[1].str();
        out.attribute = match[2].str();

        // If there's a value captured in either the quoted or unquoted capture group
        if (match[3].matched) {
            out.value = match[3].str();
            out.has_value = true;
        } else if (match[4].matched) {
            out.value = match[4].str();
            out.has_value = true;
        }
        return true;
    }
    return false;
}


// ─── element.setAttribute ────────────────────────────────────────────────────

JSValue JavascriptFunctions::js_element_set_attribute(
    JSContext* ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst* argv
) {
    if (argc < 2 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "setAttribute requires an attribute name and a value");
    }

    auto* node = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!node) return JS_EXCEPTION;

    const char* key_str = JS_ToCString(ctx, argv[0]);
    JSValue val_as_str  = JS_ToString(ctx, argv[1]);
    const char* val_str = JS_ToCString(ctx, val_as_str);

    if (key_str && val_str) {
        node->attributes[std::string(key_str)] = std::string(val_str);
        node->reconstruct = true;
    }

    JS_FreeCString(ctx, key_str);
    JS_FreeCString(ctx, val_str);
    JS_FreeValue(ctx, val_as_str);

    return JS_UNDEFINED;
}

// ─── nodeType ─────────────────────────────────────────────────────────────────

JSValue JavascriptFunctions::js_return_node_type(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic) {
    return JS_NewInt32(ctx, magic);
}

// ─── Inline event property getters/setters ────────────────────────────────────

JSValue JavascriptFunctions::js_element_get_event_property(JSContext* ctx, JSValueConst this_val, int argc, JSValue *argv, int magic) {
    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!element) return JS_UNDEFINED;

    std::string event_name = magic_to_event_name(magic);

    auto it = js_ctx.windowListeners.find(event_name);
    if (it != js_ctx.windowListeners.end()) {
        for (auto& listener : it->second) {
            if (listener.target_node == element) {
                return JS_DupValue(ctx, listener.callback);
            }
        }
    }

    return JS_NULL;
}

JSValue JavascriptFunctions::js_element_set_event_property(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc < 1) return JS_EXCEPTION;

    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!element) return JS_EXCEPTION;

    std::string event_name = magic_to_event_name(magic);

    auto it = js_ctx.windowListeners.find(event_name);
    if (it != js_ctx.windowListeners.end()) {
        auto& vec = it->second;
        vec.erase(
            std::remove_if(vec.begin(), vec.end(),
                [&](const EventListener& l) {
                    if (l.target_node == element) {
                        JS_FreeValue(ctx, l.callback);
                        return true;
                    }
                    return false;
                }),
            vec.end()
        );
    }

    if (JS_IsFunction(ctx, argv[0])) {
        js_ctx.windowListeners[event_name].push_back({
            event_name,
            JS_DupValue(ctx, argv[0]),
            element
        });
    }

    return JS_UNDEFINED;
}

// ─── element.style ────────────────────────────────────────────────────────────

JSValue JavascriptFunctions::js_element_set_style_attribute(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 2 || !JS_IsString(argv[0])) return JS_EXCEPTION;

    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!element) return JS_EXCEPTION;

    const char* key_c = JS_ToCString(ctx, argv[0]);
    const char* val_c = JS_ToCString(ctx, argv[1]);

    if (key_c && val_c) {
        element->style_properties[std::string(key_c)] = std::string(val_c);
        element->reconstruct = true;
        std::cout << "[DOM Style] Applied style: " << key_c << " = " << val_c << std::endl;
    }

    JS_FreeCString(ctx, key_c);
    JS_FreeCString(ctx, val_c);
    return JS_UNDEFINED;
}

JSValue JavascriptFunctions::js_element_get_style_attribute(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!element) return JS_UNDEFINED;

    std::stringstream ss;
    for (const auto& [key, val] : element->style_properties) {
        ss << key << ":" << val << ";";
    }

    return JS_NewString(ctx, ss.str().c_str());
}

// ─── element.ownerDocument ────────────────────────────────────────────────────

JSValue JavascriptFunctions::js_element_get_owner_document(
    JSContext* ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst* argv,
    int magic
) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue document = JS_GetPropertyStr(ctx, global, "document");
    JS_FreeValue(ctx, global);
    return document;
}

// ─── element.getRootNode ──────────────────────────────────────────────────────

Node* GetRootNode(Node* n) {
    if (!n) return nullptr;
    if (n->parent) {
        return GetRootNode(n->parent);
    }
    return n;
}

JSValue JavascriptFunctions::js_element_get_root_node(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));

    if (!element) {
        int class_id = JS_GetClassID(this_val);
        JS_ThrowInternalError(ctx,
            "getRootNode: no opaque Node* — JS class id is %d, expected %d, JS_IsObject=%d",
            (int)class_id,
            (int)QuickjsEngine::get_node_class_id(),
            (int)JS_IsObject(this_val)
        );
        return JS_EXCEPTION;
    }

    // FIX: Use centralised helper.
    auto* engine = get_engine(ctx);
    if (!engine) return JS_EXCEPTION;

    return engine->wrap_html_element(GetRootNode(element));
}
static Node *FindBody(Node *n) {
    if (n->tag == "body") return n;
    for (auto &child : n->children) {
        if (auto *res = FindBody(child.get())) return res;
    }
    return nullptr;
}
JSValue JavascriptFunctions::js_document_get_active_element(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic) {
    auto *engine = get_engine(ctx);
    // Find the focused node or fallback to body
    Node *body = FindBody(static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id())));
    Logger::Log("%s", "", 0, GetJSCallStack(ctx).c_str());
    return engine->wrap_html_element(body);
}




// JS_Functions.cpp

bool JavascriptFunctions::node_matches_selector(Node* n, const std::string& selector) {
    if (!n || n->type == NodeType::Text) return false;

    // #id
    if (selector[0] == '#') {
        return n->GetAttribute("id") == selector.substr(1);
    }

    // .class
    if (selector[0] == '.') {
        std::string want = selector.substr(1);
        std::string cls  = n->GetAttribute("class");
        // class can be space-separated list
        std::istringstream ss(cls);
        std::string token;
        while (ss >> token)
            if (token == want) return true;
        return false;
    }

    // [attr] / [attr=val] / tag[attr=val]
    AttributeSelector attr_sel;
    if (parse_attribute_selector(selector, attr_sel)) {
        // tag filter
        if (!attr_sel.tag_name.empty()) {
            std::string tag = n->tag;
            std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
            std::string want = attr_sel.tag_name;
            std::transform(want.begin(), want.end(), want.begin(), ::tolower);
            if (tag != want) return false;
        }
        if (!n->HasAttribute(attr_sel.attribute)) return false;
        if (attr_sel.has_value)
            return n->GetAttribute(attr_sel.attribute) == attr_sel.value;
        return true; // existence check only
    }

    // tag
    if (selector == "*") return true;
    std::string tag = n->tag;
    std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
    std::string want = selector;
    std::transform(want.begin(), want.end(), want.begin(), ::tolower);
    return tag == want;
}

JSValue JavascriptFunctions::js_document_query_selector(
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    if (argc < 1 || !JS_IsString(argv[0])) return JS_NULL;

    auto* engine = get_engine(ctx);
    if (!engine) return JS_NULL;

    const char* sel_c = JS_ToCString(ctx, argv[0]);
    std::string sel = sel_c ? sel_c : "";
    JS_FreeCString(ctx, sel_c);

    Node* root = js_ctx.document_node;

    std::function<Node*(Node*)> find = [&](Node* n) -> Node* {
        if (!n) return nullptr;
        if (node_matches_selector(n, sel)) return n;
        for (auto& child : n->children)
            if (auto* r = find(child.get())) return r;
        return nullptr;
    };

    Node* found = find(root);
    return found ? engine->wrap_html_element(found) : JS_NULL;
}
JSValue JavascriptFunctions::js_document_query_selector_all(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NewArray(ctx);

    const char *selector = JS_ToCString(ctx, argv[0]);
    if (!selector) return JS_NewArray(ctx);

    auto *engine = get_engine(ctx);
    Node *root = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!root) {
        // fall back to DOM root via engine
        root = js_ctx.document_node;
    }

    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;

    // Collect all matching nodes
    std::string sel(selector);
    JS_FreeCString(ctx, selector);

    std::function<void(Node*)> walk = [&](Node *n) {
        if (!n) return;
        if (node_matches_selector(n, sel)) {
            JS_SetPropertyUint32(ctx, arr, idx++, engine->wrap_html_element(n));
        }
        for (auto &child : n->children)
            walk(child.get());
    };
    walk(root);

    // Add forEach so it works like a NodeList
    // (Array already has forEach in QuickJS, but make sure it's a real array)
    return arr;
}