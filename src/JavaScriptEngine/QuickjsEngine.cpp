//
// Created by tkdtu on 5/29/2026.
//

#include "QuickjsEngine.h"

#include <filesystem>
#include <iostream>
#include <iostream>
#include <fstream>



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



QuickjsEngine::QuickjsEngine() {
    impl = std::make_unique<Impl>();
    impl->rt = JS_NewRuntime();
    if (!impl->rt) {
        throw std::runtime_error("Failed to create JS runtime");
    }

    // Initialize core system level handlers globally once for the runtime
    js_std_init_handlers(impl->rt);
}
QuickjsEngine::~QuickjsEngine() {
    // 1. Explicitly clear allocated C++ lambda tracking data blocks
    for (auto* cb : m_allocated_callbacks) {
        delete cb;
    }
    m_allocated_callbacks.clear();
    g_callback_registry.clear();

    // 2. Loop through the tracking list container and free every active tab context safely
    for (JSContext* ctx : impl->allocated_contexts) {
        if (ctx) {
            JS_FreeContext(ctx);
        }
    }
    impl->allocated_contexts.clear();

    // 3. Complete system level deallocation teardown
    js_std_free_handlers(impl->rt);
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
    if (!impl->active_ctx) {
        impl->active_ctx = ctx;
    }

    return ctx;
}

void QuickjsEngine::set_active_context(JSContext* ctx) {
    // Swap the context target pointer before execution calls or event loops execute
    impl->active_ctx = ctx;
}

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

        throw std::runtime_error("JS Error in " + filename + ": " + error_msg);
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
        std::cerr << "[Async Error]: " << (error_msg ? error_msg : "Unknown") << std::endl;
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
void QuickjsEngine::reset() {
    // Free contexts inside our tracking vector list
    for (JSContext* ctx : impl->allocated_contexts) {
        if (ctx) JS_FreeContext(ctx);
    }
    impl->allocated_contexts.clear();
    impl->active_ctx = nullptr;

    for (auto* cb : m_allocated_callbacks) {
        delete cb;
    }
    m_allocated_callbacks.clear();
    g_callback_registry.clear();

    // Re-instantiate the first baseline tracking initialization space layer
    create_tab_context();
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