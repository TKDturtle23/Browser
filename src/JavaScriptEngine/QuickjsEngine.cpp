//
// Created by tkdtu on 5/29/2026.
//

#include "QuickjsEngine.h"

#include <filesystem>
#include <iostream>
#include <fstream>

#include "JS_Functions.h"
#include "../Debug/Logger.h"
#include "Curl/BrowserCacheManager.h"
#include "Curl/UrlUtils.h"
extern "C" {
#include "quickjs.h"
#include "quickjs-libc.h"
}

struct QuickjsEngine::Impl {
 JSRuntime* rt{};
    JSContext* active_ctx = nullptr;             // Current tab context pointer
    std::vector<JSContext*> allocated_contexts;  // Registry container to track and destroy everything
};// Global/Static tracker inside QuickjsEngine to map IDs to functions instantly
static std::vector<CallbackData*> g_callback_registry;


JSClassID QuickjsEngine::s_node_class_id = 0;
std::string GetJSCallStack(JSContext *ctx) {
    std::string result;

    JSValue error = JS_NewError(ctx);
    JSValue stack_val = JS_GetPropertyStr(ctx, error, "stack");

    if (JS_IsString(stack_val)) {
        if (const char* stack_cstr = JS_ToCString(ctx, stack_val)) {
            std::istringstream stream(stack_cstr);
            std::string line;

            // Skip first line ("Error")
            std::getline(stream, line);

            // Process all stack frames
            while (std::getline(stream, line)) {
                // Optional: Skip native frames if you only want JS files
                if (line.find("(native)") != std::string::npos)
                    continue;

                std::string framedata = "";

                // Try to extract content inside "(file:line:col)"
                size_t start = line.find('(');
                size_t end = line.find(')');

                if (start != std::string::npos && end != std::string::npos && end > start) {
                    framedata = line.substr(start + 1, end - start - 1);
                } else {
                    // Fallback: Check for standard "at filename:line:col"
                    size_t at = line.find("at ");
                    if (at != std::string::npos) {
                        framedata = line.substr(at + 3);
                    } else {
                        // If it doesn't match standard QuickJS/V8 formats,
                        // just use the raw line trimmed or as-is
                        framedata = line;
                    }
                }

                // Append this frame to our total result
                if (!framedata.empty()) {
                    if (!result.empty()) {
                        result += "\n"; // Separate frames by newlines
                    }
                    result += framedata;
                }
            }

            JS_FreeCString(ctx, stack_cstr);
        }
    }

    JS_FreeValue(ctx, stack_val);
    JS_FreeValue(ctx, error);

    return result.empty() ? "<unknown>" : result;
}
std::string GetJSFileAndLine(JSContext *ctx) {
    std::string result = "<unknown>";

    JSValue error = JS_NewError(ctx);
    JSValue stack_val = JS_GetPropertyStr(ctx, error, "stack");

    if (JS_IsString(stack_val)) {
        if (const char* stack_cstr = JS_ToCString(ctx, stack_val)) {
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
static char* module_normalize(
    JSContext* ctx,
    const char* base_name,
    const char* name,
    void* opaque
) {
    try {
        auto* engine = static_cast<QuickjsEngine*>(opaque);

        std::string baseUrl = base_name ? base_name : "";
        std::string relUrl  = name ? name : "";

        // Native QuickJS modules
        if (relUrl == "os" || relUrl == "std") {
            return js_strdup(ctx, relUrl.c_str());
        }

        // QuickJS often provides fake names like:
        //   inline
        //   <loader>
        //   /src/main.jsx
        bool invalidBase =
            baseUrl.empty() ||
            baseUrl == "inline" ||
            baseUrl == "<loader>" ||
            baseUrl.find("://") == std::string::npos;

        if (invalidBase) {
            baseUrl = engine->GetActiveURL();
        }

        std::string resolved =
            Engine::Utils::ResolveUrl(baseUrl, relUrl);



        return js_strdup(ctx, resolved.c_str());
    }
    catch (...) {
        return nullptr;
    }
}

static JSModuleDef* module_loader(
    JSContext* ctx,
    const char* module_name,
    void* opaque
) {
    auto* engine = static_cast<QuickjsEngine*>(opaque);
    std::string path = module_name ? module_name : "";

    std::string source;
    if (!engine->LoadModule(path, source)) {   // <-- full path, no stripping
        JS_ThrowReferenceError(ctx, "could not load module '%s'", module_name);
        return nullptr;
    }

    // Compile only — do NOT run top-level code here. The engine still needs
    // to resolve this module's own imports and decide the evaluation order.
    JSValue func_val = JS_Eval(
        ctx,
        source.c_str(),
        source.size(),
        path.c_str(),
        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY
    );

    if (JS_IsException(func_val)) {
        JSValue exc = JS_GetException(ctx);
        const char* msg = JS_ToCString(ctx, exc);
        std::cerr << "[Module Compile Error] " << path << ": "
                   << (msg ? msg : "unknown") << "\n";
        if (msg) JS_FreeCString(ctx, msg);
        JS_FreeValue(ctx, exc);
        return nullptr;
    }
    js_module_set_import_meta(ctx, func_val, /*use_realpath*/ false, /*is_main*/ false);
    Logger::Log_Verbose("Loaded module %s", "module_loader", 0, module_name);
    auto* m = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(func_val));
    JS_FreeValue(ctx, func_val); // module itself stays alive via the engine's module table
    //Logger::Log_Debug("Module Loaded: %s", "JSEngine", 0, path.c_str());
    return m;
}
void QuickjsEngine::register_node_class() const {
    if (s_node_class_id != 0) return;

    JS_NewClassID(impl->rt, &s_node_class_id);

    JSClassDef node_class_def{};
    node_class_def.class_name = "HTMLElement";
    node_class_def.finalizer  = nullptr;

    JS_NewClass(impl->rt, s_node_class_id, &node_class_def);
}
QuickjsEngine::QuickjsEngine(BrowserCacheManager &cache) : cache(cache) {
    impl = std::make_unique<Impl>();
    impl->rt = JS_NewRuntime();
    if (!impl->rt) {
        throw std::runtime_error("Failed to create JS runtime");
    }
    JS_SetModuleLoaderFunc(impl->rt, module_normalize, module_loader, this);
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
#include <sstream>

JSContext* QuickjsEngine::create_tab_context(const std::string& URL, Node *DOM) {
    JSContext* ctx = JS_NewContext(impl->rt);
    if (!ctx) {
        std::cerr << "[Engine Error] Failed to create a fresh tab context." << std::endl;
        return nullptr;
    }

    js_std_add_helpers(ctx, 0, nullptr);

    // --- Set up Node class prototype to inherit from Object.prototype ---
    {
        JSValue tmp = JS_NewObject(ctx);
        JSValue obj_proto = JS_GetPrototype(ctx, tmp);
        JS_FreeValue(ctx, tmp);

        JSValue node_proto = JS_NewObjectProto(ctx, obj_proto);
        JS_FreeValue(ctx, obj_proto);

        JS_SetClassProto(ctx, s_node_class_id, node_proto);
    }


    js_init_module_os(ctx, "os");

    // --- Load Bootloader Script File ---
    std::string injector_script;
    std::ifstream file("Assets/BootloaderScript.js");

    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        injector_script = buffer.str();
        file.close();
    } else {
        std::cerr << "[Engine Error] Failed to open bootloader script file at: " << "Assets/BootloaderScript.js" << std::endl;
        // Fallback or cleanup depending on your requirements
        JS_FreeContext(ctx);
        return nullptr;
    }
    // Track the context in our internal list so we can destroy it at shutdown
    impl->allocated_contexts.push_back(ctx);

    // Default to setting this context as active if none is currently selected
    impl->active_ctx = ctx;

    for (auto& module : m_modules) {
        module->initialize(ctx, URL, DOM, this);
    }

    // Execute the loaded script file
    JSValue init_res = JS_Eval(ctx, injector_script.c_str(), injector_script.length(),
                               "Assets/BootloaderScript.js", JS_EVAL_TYPE_MODULE);

    if (JS_IsException(init_res)) {
        JS_FreeValue(ctx, init_res);
        log_exception(ctx, "BootloaderScript.js");
    } else {
        JS_FreeValue(ctx, init_res);
        pump_event_loop();  // let bootloader's async imports settle
    }


    return ctx;
}

void QuickjsEngine::set_active_context(JSContext* ctx, std::string URL) {
    // Swap the context target pointer before execution calls or event loops execute
    impl->active_ctx = ctx;
    ActiveURL = std::move(URL);
}
#include <source_location>
#include <utility>
void QuickjsEngine::destroy_tab_context(JSContext* ctx) const {
    if (!ctx) return;

    auto it = std::ranges::find(impl->allocated_contexts, ctx);
    if (it != impl->allocated_contexts.end()) {
        JS_FreeContext(ctx);
        impl->allocated_contexts.erase(it);
    }

    if (impl->active_ctx == ctx) {
        impl->active_ctx = impl->allocated_contexts.empty() ? nullptr : impl->allocated_contexts.front();
    }
}
void QuickjsEngine::drain_jobs() const {
    JSContext* ctx_done;
    // Microtasks
    while (JS_ExecutePendingJob(impl->rt, &ctx_done) > 0) {}

}
void QuickjsEngine::log_exception(JSContext* ctx, const std::string& filename) const {
    JSValue exception = JS_GetException(ctx);
    std::string error_msg = "Unknown error";
    std::string stack_trace;

    if (JS_IsObject(exception)) {
        JSValue name  = JS_GetPropertyStr(ctx, exception, "name");
        JSValue msg   = JS_GetPropertyStr(ctx, exception, "message");
        JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");

        const char* nc = JS_ToCString(ctx, name);
        const char* mc = JS_ToCString(ctx, msg);
        const char* sc = JS_ToCString(ctx, stack);

        if (nc && mc) error_msg = std::string(nc) + ": " + mc;
        else if (mc)  error_msg = mc;
        if (sc)       stack_trace = sc;

        JS_FreeCString(ctx, nc);
        JS_FreeCString(ctx, mc);
        JS_FreeCString(ctx, sc);
        JS_FreeValue(ctx, name);
        JS_FreeValue(ctx, msg);
        JS_FreeValue(ctx, stack);
    } else {
        const char* c = JS_ToCString(ctx, exception);
        if (c) error_msg = c;
        JS_FreeCString(ctx, c);
    }

    JS_FreeValue(ctx, exception);

    if (!stack_trace.empty())
        Logger::Log_Error("JS Error in %s:\n  %s\n%s", "[JS]", 0,
                          filename.c_str(), error_msg.c_str(), stack_trace.c_str());
    else
        Logger::Log_Error("JS Error in %s: %s", "[JS]", 0,
                          filename.c_str(), error_msg.c_str());
}
std::string QuickjsEngine::execute(const std::string& script, const std::string& filename, bool IsModule) const {
    if (!impl->active_ctx) throw std::runtime_error("No active context!");
    JSContext* ctx = impl->active_ctx;

    JSValue result = JS_Eval(ctx, script.c_str(), script.length(), filename.c_str(),
                             IsModule ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        JS_FreeValue(ctx, result);
        log_exception(ctx, filename);  // see below
        return "";
    }


    std::string output = js_value_to_string(result);
    JS_FreeValue(ctx, result);
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
        cpp_args.emplace_back(c_str ? c_str : "");
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

    auto* cb_data = new CallbackData{std::move(callback), ctx};
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
    int ret;
    while ((ret = JS_ExecutePendingJob(impl->rt, &ctx_done)) != 0) {
        if (ret < 0) {
            log_exception(ctx_done, "<microtask>");
            break; // or continue depending on how fatal you want this to be
        }
    }
    js_std_loop_once(impl->active_ctx);
    // Tick modules (timers fire here, may queue new microtasks)
    for (auto& module : m_modules) {
        module->tick();
    }

    // Drain microtasks queued by timer callbacks
    drain_jobs();

    return ret;
}
void QuickjsEngine::run_event_loop() const {
    while (pump_event_loop());
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
void QuickjsEngine::register_module(std::unique_ptr<IEngineModule> module) {
    m_modules.push_back(std::move(module));
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

void QuickjsEngine::set_element_wrapper(ElementWrapper wrapper) {
    m_element_wrapper = std::move(wrapper);
}

JSContext * QuickjsEngine::get_active_context() const {
    return impl->active_ctx;
}

bool QuickjsEngine::LoadModule(const std::string &path, std::string &out_src) const {
    out_src = cache.GetResource(path, true);
    if (out_src.empty()) return false;
    return true;
}
std::unordered_map<Node*, JSValue> wrapperCache{};

JSValue QuickjsEngine::wrap_html_element(Node* element) const {
    if (!element || !impl->active_ctx) return JS_NULL;
    JSContext* ctx = impl->active_ctx;

    auto it = wrapperCache.find(element);
    if (it != wrapperCache.end()) {
        // Return a fresh reference incremented explicitly for the JS runtime execution stack
        return JS_DupValue(ctx, it->second);
    }

    JSValue js_element = JS_NewObjectClass(ctx, s_node_class_id);
    if (JS_IsException(js_element)) return JS_NULL;

    JS_SetOpaque(js_element, element);

    if (m_element_wrapper)
        m_element_wrapper(ctx, js_element, element);

    // Keep one reference permanently alive on the C++ side inside our cache map
    wrapperCache[element] = JS_DupValue(ctx, js_element);

    // Return the original value reference to JavaScript
    return js_element;
}
JSClassID QuickjsEngine::get_node_class_id() {
    return s_node_class_id;
}
