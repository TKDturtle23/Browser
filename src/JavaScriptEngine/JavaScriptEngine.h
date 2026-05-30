//
// Created by tkdtu on 5/29/2026.
//

#ifndef BROWSER_JAVASCRIPTENGINE_H
#define BROWSER_JAVASCRIPTENGINE_H
#include <memory>
#include <string>

#include "QuickjsEngine.h"


class JavaScriptEngine {
public:
JavaScriptEngine();
    ~JavaScriptEngine();
    void InjectData();
    void Run(const std::string& script_data);

    bool Step() const;
    void RunAll() const;
    void Reset();

    JSContext *create_tab_context();

    void set_active_context(JSContext *ctx);

    void destroy_tab_context(JSContext *ctx);
    JavaScriptEngine(const JavaScriptEngine&) = delete;
    JavaScriptEngine& operator=(const JavaScriptEngine&) = delete;
private:
QuickjsEngine qjs_engine;
};




#endif //BROWSER_JAVASCRIPTENGINE_H
