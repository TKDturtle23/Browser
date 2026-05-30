#include "JavaScriptEngine.h"

#include <iostream>

std::string Test_function(const std::string& str) {
    return str + "Testing worked!";
}
JavaScriptEngine::JavaScriptEngine() {

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

std::string JavaScriptEngine::Run(const std::string &script_data, const std::string &script_name) {
    return qjs_engine.execute(script_data, script_name);
}

bool JavaScriptEngine::Step() const {
    return qjs_engine.pump_event_loop();
}

void JavaScriptEngine::RunAll() const {
    qjs_engine.run_event_loop();
}



JSContext * JavaScriptEngine::create_tab_context() {
    return qjs_engine.create_tab_context();
}

void JavaScriptEngine::set_active_context(JSContext *ctx) {
    qjs_engine.set_active_context(ctx);
}



void JavaScriptEngine::destroy_tab_context(JSContext *ctx) {
    qjs_engine.destroy_tab_context(ctx);
}


