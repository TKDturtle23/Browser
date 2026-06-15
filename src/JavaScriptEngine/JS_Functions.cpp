//
// Created by tkdtu on 5/29/2026.
//

#include "JS_Functions.h"

#include <iostream>
#include <sstream>
#include <string>

#include "../Debug/Logger.h"
#include "quickjs.h"
#include "QuickjsEngine.h"

#include "Parser.h"
#include "Tokenizer.h"
#include "NodeToHTML.h"
JavascriptContext js_ctx;

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

// 1. Internal C++ Trampoline for document.getElementById
 JSValue JavascriptFunctions::js_document_get_element_by_id(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_NULL;
    }

    const char* id_c_str = JS_ToCString(ctx, argv[0]);
    std::string search_id = id_c_str ? id_c_str : "";
    JS_FreeCString(ctx, id_c_str);

    std::cout << "[DOM Bridge] JavaScript requested element ID: " << search_id << std::endl;

    // --- MOCK DATABASE LOOKUP ---
    // In your real browser, you'd do: MockHTMLElement* el = my_html_parser.find_by_id(search_id);
    if (!js_ctx.document_node) {
        return JS_NULL;
    }
    Node* mock_element = Find_Element_By_ID(search_id, js_ctx.document_node);
    if (!mock_element) {
        return JS_NULL;
    }
    if (search_id == mock_element->id) {
        // Find the engine instance pointer via context or pass it along.
        // For this bridge wrapper, we call our tracking generator directly:
        JSValue global_obj = JS_GetGlobalObject(ctx);
        JSValue engine_ptr_val = JS_GetPropertyStr(ctx, global_obj, "__engine_internal_ptr");
        int64_t raw_address = 0;
        JS_ToBigInt64(ctx, &raw_address, engine_ptr_val);

        auto* engine = reinterpret_cast<QuickjsEngine*>(static_cast<uintptr_t>(raw_address));
        JS_FreeValue(ctx, engine_ptr_val);
        JS_FreeValue(ctx, global_obj);

        return engine->wrap_html_element(mock_element);
    }

    return JS_NULL;
}

// 2. Internal C++ Getters/Setters for HTML Element Properties
 JSValue JavascriptFunctions::js_element_get_property(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    // Extract the raw C++ pointer hidden inside the JavaScript object
    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id())); // Class ID 1 for elements
    if (!element) return JS_UNDEFINED;

    switch (magic) {
        case 0: return JS_NewString(ctx, element->tag.c_str());
        case 1: return JS_NewString(ctx, element->id.c_str());
        case 2: return JS_NewString(ctx, element->class_name.c_str());
        case 3: return JS_NewString(ctx, NodeToHTML::GetHTML(element).c_str());
        default: return JS_UNDEFINED;
    }
}

 JSValue JavascriptFunctions::js_element_set_property(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    auto* element = static_cast<Node*>(JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
    if (!element || argc < 1) return JS_EXCEPTION;

    // The assigned right-hand value is always passed inside argv[0]
    const char* c_str = JS_ToCString(ctx, argv[0]);
    std::string value_str = c_str ? c_str : "";
    JS_FreeCString(ctx, c_str);

    switch (magic) {
        case 1: element->id = value_str; break;
        case 2: element->class_name = value_str; break;
        case 3: {

            Tokenizer tokenizer;
            auto tokens = tokenizer.tokenize(value_str);
            Parser parser;
            auto sec = parser.Parse(tokens);
            element->children = std::move(sec.children);
            element->reconstruct = true;
            std::cout << "[DOM Engine] Mutation Detected! New innerHTML: " << value_str << std::endl;
            break;
        }

    }
    return JS_UNDEFINED;
}

JSValue JavascriptFunctions::js_document_get_data(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic) {
    switch (magic) {
        case (0): {
            return js_ctx.title ? JS_NewString(ctx, js_ctx.title->c_str()) : JS_UNDEFINED;
        }
            case (1): {
            auto body = Find_Body(js_ctx.document_node);
            return body ? JS_NewString(ctx, NodeToHTML::GetHTML(body).c_str()) : JS_UNDEFINED;
        }
        default: return JS_UNDEFINED;
    }
    return JS_UNDEFINED;
}

JSValue JavascriptFunctions::js_document_set_data(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_EXCEPTION;
    }

    switch (magic) {
        case (0): { // title
            const char* c_str = JS_ToCString(ctx, argv[0]);
            std::string value_str = c_str ? c_str : "";
            JS_FreeCString(ctx, c_str);
            js_ctx.title = new std::string(value_str);
            return JS_UNDEFINED;
        }
            case (1): { // document.body
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
            }
    }
return JS_UNDEFINED;

}

static std::string GetJSFileAndLine(JSContext* ctx) {
    std::string result = "<unknown>";

    JSValue error = JS_NewError(ctx);
    JSValue stack_val = JS_GetPropertyStr(ctx, error, "stack");

    if (JS_IsString(stack_val)) {
        const char* stack_cstr = JS_ToCString(ctx, stack_val);

        if (stack_cstr) {
            std::istringstream stream(stack_cstr);
            std::string line;

            // Skip first line ("Error")
            std::getline(stream, line);

            // Find first NON-native stack frame
            while (std::getline(stream, line)) {
                // Ignore native calls
                if (line.find("(native)") != std::string::npos)
                    continue;

                // Find "(file:line:col)"
                size_t start = line.find('(');
                size_t end = line.find(')');

                if (start != std::string::npos &&
                    end != std::string::npos &&
                    end > start)
                {
                    result = line.substr(start + 1, end - start - 1);
                    break;
                }

                // Fallback:
                // at sandbox.js:1:8
                size_t at = line.find("at ");
                if (at != std::string::npos) {
                    result = line.substr(at + 3);
                    break;
                }
            }

            JS_FreeCString(ctx, stack_cstr);
        }
    }

    JS_FreeValue(ctx, stack_val);
    JS_FreeValue(ctx, error);

    return result;
}

JSValue JavascriptFunctions::js_console_log(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic) {
    if (argc < 1) return JS_UNDEFINED;
    const char* c_str = JS_ToCString(ctx, argv[0]);
    std::string value_str = c_str ? c_str : "";
    JS_FreeCString(ctx, c_str);
    for (int i = 0; i < js_ctx.GroupLevel; i++) {
        value_str = "    " + value_str;
    }

    switch (magic) {
        case (0): { // log
            Logger::Log(c_str, GetJSFileAndLine(ctx), js_ctx.GroupLevel); break;
        }
        case (1): { // info
            Logger::Log_Info(c_str, GetJSFileAndLine(ctx), js_ctx.GroupLevel); break;
        }
        case (2): { // warn
            Logger::Log_Warning(c_str, GetJSFileAndLine(ctx), js_ctx.GroupLevel); break;
        }
        case (3): { // error
            Logger::Log_Error(c_str, GetJSFileAndLine(ctx), js_ctx.GroupLevel); break;
        }
        case (4): { // debug
            Logger::Log_Debug(c_str, GetJSFileAndLine(ctx), js_ctx.GroupLevel); break;
        }
            case(5): { // group, group collapsed (temp)
            js_ctx.GroupLevel++; break;
        }
            case(6): { // groupEnd
            js_ctx.GroupLevel--; break;
        }
        default: Logger::Log(c_str, GetJSFileAndLine(ctx), js_ctx.GroupLevel); break;
    }

    return JS_UNDEFINED;
}

