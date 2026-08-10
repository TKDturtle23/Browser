#include "JavaScriptEngine.h"

#include <iostream>
#include <utility>

#include "Modules/ConsoleBridge.h"
#include "Modules/DOMBridge.h"

std::string Test_function(const std::string& str) {
    return str + "Testing worked!";
}
JavaScriptEngine::JavaScriptEngine(BrowserCacheManager &cache) : qjs_engine(cache) {
    qjs_engine.register_module(std::make_unique<DOMBridge>(qjs_engine));
    qjs_engine.register_module(std::make_unique<ConsoleBridge>());
}

JavaScriptEngine::~JavaScriptEngine() {

}

void JavaScriptEngine::InjectData() {
    qjs_engine.inject_global_string("browserInfo", "name", "Euclase Browser");
    qjs_engine.inject_global_string("browserInfo", "version", "1.0");
    qjs_engine.register_function("test", [&](const JSArgs& args) -> std::string {
        if (args.empty()) return "Error: no string provided";

        std::string str = args[0];
        std::cout << "Received string: " << str << std::endl;
        return Test_function(str);
    });
    qjs_engine.inject_nested_string("browser.identity.name", "Euclase Core");
    qjs_engine.inject_nested_string("browser.identity.vendor.codename", "Gemstone");

    qjs_engine.register_nested_function("browser.tabs.utils.test", [](const JSArgs& args) -> std::string {
    std::cout << "-> Deeply nested layout execution triggered successfully!" << std::endl;


    return "Nested Bridge Action Succeeded!";
});


}

std::string JavaScriptEngine::Run(const std::string &script_data, const std::string &script_name, bool IsModule) {
    return qjs_engine.execute(script_data, script_name, IsModule);
}

bool JavaScriptEngine::Step() const {
    return qjs_engine.pump_event_loop();
}

void JavaScriptEngine::RunAll() const {
    qjs_engine.run_event_loop();
}



JSContext * JavaScriptEngine::create_tab_context(std::string URL, Node *DOM) {
    return qjs_engine.create_tab_context(std::move(URL),  DOM);
}

void JavaScriptEngine::DispatchEvent(const std::vector<EventListener> &Listeners, const std::string &type) {
    auto ctx = qjs_engine.get_active_context();
    JSValue eventObj = JS_NewObject(ctx);

    JS_SetPropertyStr(
        ctx,
        eventObj,
        "type",
        JS_NewString(ctx, type.c_str()));

    for (auto& listener : Listeners)
    {
        JS_Call(
            ctx,
            listener.callback,
            JS_UNDEFINED,
            1,
            &eventObj);
    }

    JS_FreeValue(ctx, eventObj);
}

void JavaScriptEngine::set_active_context(JSContext *ctx, const std::string &url) {
    qjs_engine.set_active_context(ctx, url);
}



void JavaScriptEngine::destroy_tab_context(JSContext *ctx) {
    qjs_engine.destroy_tab_context(ctx);
}


