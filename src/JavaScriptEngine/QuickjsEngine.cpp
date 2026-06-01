//
// Created by tkdtu on 5/29/2026.
//

#include "QuickjsEngine.h"

#include <filesystem>
#include <iostream>
#include <iostream>
#include <fstream>

#include "JS_Functions.h"
#include "../Debug/Logger.h"


extern "C" {
#include "quickjs.h"
#include "quickjs-libc.h"
}

struct QuickjsEngine::Impl {
 JSRuntime* rt;
    JSContext* active_ctx = nullptr;             // Current tab context pointer
    std::vector<JSContext*> allocated_contexts;  // Registry container to track and destroy everything
};// Global/Static tracker inside QuickjsEngine to map IDs to functions instantly
static std::vector<CallbackData*> g_callback_registry;

JSClassID QuickjsEngine::s_node_class_id = 0;
void QuickjsEngine::register_node_class() {
    if (s_node_class_id != 0) return; // already registered

    JS_NewClassID(impl->rt, &s_node_class_id);

    JSClassDef node_class_def{};
    node_class_def.class_name = "Node";
    node_class_def.finalizer  = nullptr; // C++ owns the Node lifetime, not the GC

    JS_NewClass(impl->rt, s_node_class_id, &node_class_def);
}
QuickjsEngine::QuickjsEngine() {
    impl = std::make_unique<Impl>();
    impl->rt = JS_NewRuntime();
    if (!impl->rt) {
        throw std::runtime_error("Failed to create JS runtime");
    }

    // Initialize core system level handlers globally once for the runtime
    js_std_init_handlers(impl->rt);

    register_node_class();
}
QuickjsEngine::~QuickjsEngine() {
    // 1. Explicitly clear allocated C++ lambda tracking data blocks
    for (auto* cb : m_allocated_callbacks) {
        delete cb;
    }
    m_allocated_callbacks.clear();
    g_callback_registry.clear();

    // 2. STOP system handlers first while contexts are still alive
    js_std_free_handlers(impl->rt);

    // 3. Loop through the tracking list container and free contexts safely
    for (JSContext* ctx : impl->allocated_contexts) {
        if (ctx) {
            JS_FreeContext(ctx);
        }
    }
    impl->allocated_contexts.clear();

    // 4. Finally, destroy the core runtime
    JS_FreeRuntime(impl->rt);
}
JSContext* QuickjsEngine::create_tab_context() {
    JSContext* ctx = JS_NewContext(impl->rt);
    if (!ctx) {
        std::cerr << "[Engine Error] Failed to create a fresh tab context." << std::endl;
        return nullptr;
    }

    // Initialize core helpers and OS layout structure for this specific context environment
    js_std_add_helpers(ctx, 0, nullptr);

    extern JSModuleDef *js_init_module_os(JSContext *ctx, const char *module_name);
    js_init_module_os(ctx, "os");

    // Inject your standard library runtime shims
    std::string injector_script =
        "import * as nativeOs from 'os';\n"
        "globalThis.setTimeout = nativeOs.setTimeout;\n"
        "globalThis.setInterval = nativeOs.setInterval;\n"
        "globalThis.clearTimeout = nativeOs.clearTimeout;\n";

    JSValue init_res = JS_Eval(ctx, injector_script.c_str(), injector_script.length(),
                               "<loader>", JS_EVAL_TYPE_MODULE);

    if (JS_IsException(init_res)) {
        JSValue exception = JS_GetException(ctx);
        const char* error_c_str = JS_ToCString(ctx, exception);
        std::cerr << "[Engine Critical Error]: Tab environment module configuration failed: "
                  << (error_c_str ? error_c_str : "Unknown") << std::endl;
        JS_FreeCString(ctx, error_c_str);
        JS_FreeValue(ctx, exception);
    }
    JS_FreeValue(ctx, init_res);

    // Track the context in our internal list so we can destroy it at shutdown
    impl->allocated_contexts.push_back(ctx);

    // Default to setting this context as active if none is currently selected

        impl->active_ctx = ctx;


    initialize_dom_bridge();
    initialize_console();
    return ctx;
}

void QuickjsEngine::set_active_context(JSContext* ctx) {
    // Swap the context target pointer before execution calls or event loops execute
    impl->active_ctx = ctx;
}
#include <source_location>
void QuickjsEngine::destroy_tab_context(JSContext* ctx) {
    if (!ctx) return;

    auto it = std::find(impl->allocated_contexts.begin(), impl->allocated_contexts.end(), ctx);
    if (it != impl->allocated_contexts.end()) {
        JS_FreeContext(ctx);
        impl->allocated_contexts.erase(it);
    }

    if (impl->active_ctx == ctx) {
        impl->active_ctx = impl->allocated_contexts.empty() ? nullptr : impl->allocated_contexts.front();
    }
}
std::string QuickjsEngine::execute(const std::string& script, const std::string& filename) {
    if (!impl->active_ctx) throw std::runtime_error("No active script execution engine target context selected!");

    JSValue result = JS_Eval(impl->active_ctx, script.c_str(), script.length(), filename.c_str(), JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(impl->active_ctx);
        const char* error_c_str = JS_ToCString(impl->active_ctx, exception);
        std::string error_msg = error_c_str ? error_c_str : "Unknown JavaScript Error";

        JS_FreeCString(impl->active_ctx, error_c_str);
        JS_FreeValue(impl->active_ctx, exception);
        JS_FreeValue(impl->active_ctx, result);


        Logger::Log_Error("JS Error in %s: %s", "[JS]", 0,filename.c_str(), error_msg.c_str());

    }

    std::string output = js_value_to_string(result);
    JS_FreeValue(impl->active_ctx, result);
    return output;
}
void QuickjsEngine::inject_global_string(const std::string& object_name, const std::string& property_name, const std::string& value) const {
    if (!impl->active_ctx) return;
    JSContext* ctx = impl->active_ctx;

    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue target_obj = JS_GetPropertyStr(ctx, global_obj, object_name.c_str());

    if (JS_IsUndefined(target_obj) || JS_IsNull(target_obj) || JS_IsException(target_obj)) {
        JS_FreeValue(ctx, target_obj);
        target_obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, global_obj, object_name.c_str(), JS_DupValue(ctx, target_obj));
    }

    JSValue js_str = JS_NewString(ctx, value.c_str());
    JS_DefinePropertyValueStr(ctx, target_obj, property_name.c_str(), js_str, JS_PROP_C_W_E);

    JS_FreeValue(ctx, target_obj);
    JS_FreeValue(ctx, global_obj);
}
std::string QuickjsEngine::js_value_to_string(const JSValue value) const {
    if (!impl->active_ctx) return "undefined";
    if (JS_IsUndefined(value)) return "undefined";
    if (JS_IsNull(value)) return "null";

    const char* c_str = JS_ToCString(impl->active_ctx, value);
    std::string result = c_str ? c_str : "";
    JS_FreeCString(impl->active_ctx, c_str);
    return result;
}


static JSValue cpp_callback_trampoline(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    // 'magic' is the direct vector index of our C++ callback!
    // No string parsing, no opaque object lookups, pure speed.

    // We need a way to look back at our engine instance.
    // We can fetch the engine instance from the context or use a clean global registry tracker.
    // For simplicity, let's assume a global index lookup or access via engine mapping:
    auto* cb_data = QuickjsEngine::get_callback_by_index(magic);
    if (!cb_data || !cb_data->callback) {
        return JS_ThrowReferenceError(ctx, "Bridge Engine Error: Core callback missing.");
    }

    // Convert parameters safely (Arguments are JSValueConst, so do NOT free them!)
    JSArgs cpp_args;
    for (int i = 0; i < argc; ++i) {
        const char* c_str = JS_ToCString(ctx, argv[i]);
        cpp_args.push_back(c_str ? c_str : "");
        JS_FreeCString(ctx, c_str); // Free the temporary C-string, not the JSValue!
    }

    // Execute C++ logic
    std::string cpp_result = cb_data->callback(cpp_args);

    // Return a live, newly allocated JSValue as required by the docs
    return JS_NewString(ctx, cpp_result.c_str());
}


CallbackData* QuickjsEngine::get_callback_by_index(int index) {
    if (index >= 0 && index < g_callback_registry.size()) {
        return g_callback_registry[index];
    }
    return nullptr;
}
void QuickjsEngine::register_function(const std::string &name, JSCallback callback) {
    if (!impl->active_ctx) return;
    JSContext* ctx = impl->active_ctx;

    auto* cb_data = new CallbackData{callback, ctx};
    m_allocated_callbacks.push_back(cb_data);

    g_callback_registry.push_back(cb_data);
    int callback_id = static_cast<int>(g_callback_registry.size() - 1);

    JSValue js_func = JS_NewCFunction2(
        ctx,
        reinterpret_cast<JSCFunction*>(cpp_callback_trampoline),
        name.c_str(),
        0,
        JS_CFUNC_generic_magic,
        callback_id
    );

    if (JS_IsException(js_func)) {
        std::cerr << "[ERROR] JS_NewCFunction2 failed!" << std::endl;
        return;
    }

    JSValue global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, name.c_str(), js_func);
    JS_FreeValue(ctx, global_obj);
}



JSValue QuickjsEngine::resolve_or_create_path(const std::string& path, std::string& out_final_key) const {
    if (!impl->active_ctx) return JS_UNDEFINED;
    JSContext* ctx = impl->active_ctx;

    std::stringstream ss(path);
    std::string item;
    std::vector<std::string> tokens;

    while (std::getline(ss, item, '.')) {
        if (!item.empty()) tokens.push_back(item);
    }

    if (tokens.empty()) {
        out_final_key = "";
        return JS_UNDEFINED;
    }

    out_final_key = tokens.back();
    tokens.pop_back();

    JSValue current_obj = JS_GetGlobalObject(ctx);

    for (const auto& token : tokens) {
        JSValue next_obj = JS_GetPropertyStr(ctx, current_obj, token.c_str());

        if (JS_IsUndefined(next_obj) || JS_IsNull(next_obj) || JS_IsException(next_obj)) {
            JS_FreeValue(ctx, next_obj);
            next_obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, current_obj, token.c_str(), JS_DupValue(ctx, next_obj));
        }

        JS_FreeValue(ctx, current_obj);
        current_obj = next_obj;
    }

    return current_obj;
}


bool QuickjsEngine::pump_event_loop() const {
    if (!impl->active_ctx) return false;
    JSContext* ctx_done;

    int job_status = JS_ExecutePendingJob(impl->rt, &ctx_done);
    if (job_status < 0) {
        JSValue exception = JS_GetException(ctx_done);
        const char* error_msg = JS_ToCString(ctx_done, exception);
        Logger::Log_Error("JS Error: %s", std::to_string(std::source_location::current().line()), 0, error_msg);

        JS_FreeCString(ctx_done, error_msg);
        JS_FreeValue(ctx_done, exception);
        return false;
    }

    // Update time loops relative directly to our current context pointer focus loop layer
    js_std_loop(impl->active_ctx);

    return (job_status > 0);
}

void QuickjsEngine::run_event_loop() const {
    bool working = true;
    while (working) {
        bool more_microtasks = pump_event_loop();
        if (!more_microtasks) {
            working = false;
        }
    }
}


void QuickjsEngine::register_nested_function(const std::string& path, const JSCallback &callback) {
    if (!impl->active_ctx) return;
    JSContext* ctx = impl->active_ctx;

    std::string final_function_name;
    JSValue parent_obj = resolve_or_create_path(path, final_function_name);

    if (final_function_name.empty() || JS_IsUndefined(parent_obj)) {
        JS_FreeValue(ctx, parent_obj);
        return;
    }

    auto* cb_data = new CallbackData{callback, ctx};
    m_allocated_callbacks.push_back(cb_data);
    g_callback_registry.push_back(cb_data);
    int callback_id = static_cast<int>(g_callback_registry.size() - 1);

    JSValue js_func = JS_NewCFunction2(
        ctx,
        reinterpret_cast<JSCFunction*>(cpp_callback_trampoline),
        final_function_name.c_str(),
        0,
        JS_CFUNC_generic_magic,
        callback_id
    );

    JS_SetPropertyStr(ctx, parent_obj, final_function_name.c_str(), js_func);
    JS_FreeValue(ctx, parent_obj);
}

void QuickjsEngine::inject_nested_string(const std::string& path, const std::string& value) const {
    if (!impl->active_ctx) return;
    JSContext* ctx = impl->active_ctx;

    std::string final_property_name;
    JSValue parent_obj = resolve_or_create_path(path, final_property_name);

    if (final_property_name.empty() || JS_IsUndefined(parent_obj)) {
        JS_FreeValue(ctx, parent_obj);
        return;
    }

    JSValue js_str = JS_NewString(ctx, value.c_str());
    JS_DefinePropertyValueStr(ctx, parent_obj, final_property_name.c_str(), js_str, JS_PROP_C_W_E);
    JS_FreeValue(ctx, parent_obj);
}
void QuickjsEngine::initialize_dom_bridge() {
    if (!impl->active_ctx) {
        std::cerr << "[DOM Bridge Error] Cannot initialize: active_ctx is NULL!" << std::endl;
        return;
    }

    JSContext* ctx = impl->active_ctx;
    JSValue global_obj = JS_GetGlobalObject(ctx);

    // Bind internal engine pointer for native callbacks that need to reach back into C++
    JS_SetPropertyStr(ctx, global_obj, "__engine_internal_ptr",
                      JS_NewBigInt64(ctx, reinterpret_cast<int64_t>(this)));

    // Create the document object and attach its native function bindings
    JSValue document_obj = JS_NewObject(ctx);

    JSValue get_element_fn = JS_NewCFunction(
        ctx, JavascriptFunctions::js_document_get_element_by_id, "getElementById", 1);

    JS_DefinePropertyGetSet(ctx, document_obj, JS_NewAtom(ctx, "title"),
    JS_NewCFunctionMagic(ctx, JavascriptFunctions::js_document_get_data, "get", 0, JS_CFUNC_generic_magic, 0),
    JS_NewCFunctionMagic(ctx, JavascriptFunctions::js_document_set_data, "set", 1, JS_CFUNC_generic_magic, 0),
    JS_PROP_C_W_E);


    if (JS_IsException(get_element_fn)) {
        std::cerr << "[DOM Bridge Error] Failed to create native JSCFunction for getElementById!" << std::endl;
    } else {
        JS_SetPropertyStr(ctx, document_obj, "getElementById", get_element_fn);
    }

    // Attach the fully constructed document object onto globalThis
    JS_SetPropertyStr(ctx, global_obj, "document", document_obj);


    // Now, copy 'document' properties or anchor it to the window object context
    std::string final_key;
    // Alias 'window' to point directly to the global object itself!
    JS_SetPropertyStr(ctx, global_obj, "window", JS_DupValue(ctx, global_obj));
    JS_SetPropertyStr(ctx, global_obj, "self", JS_DupValue(ctx, global_obj));

    // Now, copy 'document' properties or anchor it to the window object context
    JS_SetPropertyStr(ctx, global_obj, "document", JS_DupValue(ctx, document_obj));

    JS_FreeValue(ctx, global_obj);
}
void AddFunction(JSCFunctionMagic* func, const std::string& name, JSValue object, JSContext *ctx, int magic) {
    // 1. Generate the native Magic C function callback wrapper
    JSValue native_fn = JS_NewCFunctionMagic(
        ctx, func, name.c_str(), 1, JS_CFUNC_generic_magic, magic);

    if (JS_IsException(native_fn)) {
        std::cerr << "[AddFunction Error] Failed to create native JSCFunction for: " << name << std::endl;
        return;
    }

    // 2. ✅ FIXED: Bind the function directly onto the target console 'object' container
    // JS_SetPropertyStr cleanly takes ownership of native_fn's refcount here.
    JS_SetPropertyStr(ctx, object, name.c_str(), native_fn);
}
void QuickjsEngine::initialize_console() const {
    if (!impl->active_ctx) {
        std::cerr << "[Console Bridge Error] Cannot initialize: active_ctx is NULL!" << std::endl;
        return;
    }

    JSContext* ctx = impl->active_ctx;
    JSValue global_obj = JS_GetGlobalObject(ctx);

    JSValue console_obj = JS_NewObject(ctx);
    AddFunction(JavascriptFunctions::js_console_log, "log", console_obj, ctx, 0);
    AddFunction(JavascriptFunctions::js_console_log, "info", console_obj, ctx, 1);
    AddFunction(JavascriptFunctions::js_console_log, "warn", console_obj, ctx, 2);
    AddFunction(JavascriptFunctions::js_console_log, "error", console_obj, ctx, 3);
    AddFunction(JavascriptFunctions::js_console_log, "debug", console_obj, ctx, 4);
    AddFunction(JavascriptFunctions::js_console_log, "group", console_obj, ctx, 5);
    AddFunction(JavascriptFunctions::js_console_log, "groupCollapsed", console_obj, ctx, 5);
    AddFunction(JavascriptFunctions::js_console_log, "groupEnd", console_obj, ctx, 6);

    // Attach the fully constructed document object onto globalThis
    JS_SetPropertyStr(ctx, global_obj, "console", console_obj);
    JS_FreeValue(ctx, global_obj);
}

JSValue QuickjsEngine::wrap_html_element(Node* element) const {
    if (!element) return JS_NULL;
    if (!impl->active_ctx) return JS_NULL;
    JSContext* ctx = impl->active_ctx;

    // Must use a classed object — plain JS_NewObject has no opaque slot
    JSValue js_element = JS_NewObjectClass(ctx, s_node_class_id);
    if (JS_IsException(js_element)) return JS_NULL;

    // Now the opaque slot exists and this pointer will survive get/set callbacks
    JS_SetOpaque(js_element, element);

    JS_DefinePropertyGetSet(ctx, js_element, JS_NewAtom(ctx, "tagName"),
        JS_NewCFunctionMagic(ctx, JavascriptFunctions::js_element_get_property, "get", 0, JS_CFUNC_generic_magic, 0),
        JS_UNDEFINED, JS_PROP_C_W_E);

    JS_DefinePropertyGetSet(ctx, js_element, JS_NewAtom(ctx, "id"),
        JS_NewCFunctionMagic(ctx, JavascriptFunctions::js_element_get_property, "get", 0, JS_CFUNC_generic_magic, 1),
        JS_NewCFunctionMagic(ctx, JavascriptFunctions::js_element_set_property, "set", 1, JS_CFUNC_generic_magic, 1),
        JS_PROP_C_W_E);

    JS_DefinePropertyGetSet(ctx, js_element, JS_NewAtom(ctx, "className"),
        JS_NewCFunctionMagic(ctx, JavascriptFunctions::js_element_get_property, "get", 0, JS_CFUNC_generic_magic, 2),
        JS_NewCFunctionMagic(ctx, JavascriptFunctions::js_element_set_property, "set", 1, JS_CFUNC_generic_magic, 2),
        JS_PROP_C_W_E);

    JS_DefinePropertyGetSet(ctx, js_element, JS_NewAtom(ctx, "innerHTML"),
        JS_NewCFunctionMagic(ctx, JavascriptFunctions::js_element_get_property, "get", 0, JS_CFUNC_generic_magic, 3),
        JS_NewCFunctionMagic(ctx, JavascriptFunctions::js_element_set_property, "set", 1, JS_CFUNC_generic_magic, 3),
        JS_PROP_C_W_E);

    return js_element;
}

JSClassID QuickjsEngine::get_node_class_id() {
    return s_node_class_id;
}
