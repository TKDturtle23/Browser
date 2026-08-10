//
// Created by tkdtu on 5/29/2026.
//

#ifndef BROWSER_QUICKJSENGINE_H
#define BROWSER_QUICKJSENGINE_H
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "Curl/BrowserCacheManager.h"
#include "Modules/IEngineModule.h"
#include "../Node/Node.h"
#include "quickjs.h"
#include "Modules/Vars/Timers.h"

struct MockHTMLElement;

// Custom exception class for handling JS runtime/compile errors elegantly
class JavaScriptException : public std::runtime_error {
public:
    explicit JavaScriptException(const std::string& message) : std::runtime_error(message) {}
};

// Define a clean C++ type for our JavaScript arguments
using JSArgs = std::vector<std::string>;
// Define the signature for our C++ callbacks
using JSCallback = std::function<std::string(const JSArgs&)>;
struct CallbackData {
    JSCallback callback;
    JSContext* ctx;
};
std::string GetJSFileAndLine(JSContext* ctx);
std::string GetJSCallStack(JSContext *ctx);
class QuickjsEngine {
public:
    void register_node_class() const;

    QuickjsEngine(BrowserCacheManager &cache);

    ~QuickjsEngine();


    // Prevent copying to avoid accidental double-freeing of the JS runtime pointers
    QuickjsEngine(const QuickjsEngine&) = delete;
    QuickjsEngine& operator=(const QuickjsEngine&) = delete;
    JSContext *create_tab_context(const std::string& URL, Node *DOM);

    void set_active_context(JSContext *ctx, std::string URL);
    [[nodiscard]] std::string GetActiveURL() const { return ActiveURL; };
    void destroy_tab_context(JSContext *ctx) const;

    void drain_jobs() const;

    void log_exception(JSContext *ctx, const std::string &filename) const;

    // Execute a raw string of JavaScript code. Returns the result as a std::string.
    // Throws JavaScriptException if something goes wrong.
    std::string execute(const std::string& script, const std::string& filename , bool IsModule) const;

    // Helper to inject a global string property (like browserInfo properties)
    void inject_global_string(const std::string& object_name, const std::string& property_name, const std::string& value) const;

    std::string js_value_to_string(JSValue value) const;

    static CallbackData *get_callback_by_index(int index);

    // New: Register a C++ callback function globally in JS
    void register_function(const std::string& name, JSCallback callback);

    // Internal trampoline structure to bridge C++ std::function with raw C pointers
    void register_nested_function(const std::string& path, const JSCallback &callback);
    void inject_nested_string(const std::string& path, const std::string& value) const;

    // Processes any pending microtasks (Promises) or macrotasks (timers)
    // Returns true if there is still more async work pending in the future
    bool pump_event_loop() const;

    // Fully executes the event loop until absolutely all async jobs are finished
    void run_event_loop() const;

    std::vector<CallbackData*> m_allocated_callbacks; // For tracking and cleanup

    void initialize_dom_bridge();
    void initialize_console() const;
    JSValue wrap_html_element(Node* element) const;
    static JSClassID get_node_class_id();
    // Remove initialize_dom_bridge(), initialize_console(), wrap_html_element() from public API
    // Add:
    void register_module(std::unique_ptr<IEngineModule> module);
    using ElementWrapper = std::function<void(JSContext*, JSValue, Node*)>;

    // DOMBridge calls this during initialize():
    void set_element_wrapper(ElementWrapper wrapper);
    JSContext* get_active_context() const;
    struct Impl;
    std::unique_ptr<Impl> impl;
    bool LoadModule(const std::string& path, std::string& out_src) const;
private:
    Timers timers_;
    std::string ActiveURL;
    BrowserCacheManager &cache;
    std::vector<std::unique_ptr<IEngineModule>> m_modules;
    JSValue resolve_or_create_path(const std::string& path, std::string& out_final_key) const;


    static JSClassID s_node_class_id;
    ElementWrapper m_element_wrapper;


};


#endif //BROWSER_QUICKJSENGINE_H
