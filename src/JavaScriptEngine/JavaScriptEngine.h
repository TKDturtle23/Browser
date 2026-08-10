//
// Created by tkdtu on 5/29/2026.
//

#ifndef BROWSER_JAVASCRIPTENGINE_H
#define BROWSER_JAVASCRIPTENGINE_H
#include <memory>
#include <string>

#include "JS_Functions.h"
#include "QuickjsEngine.h"
#include "Curl/BrowserCacheManager.h"


class JavaScriptEngine {
public:
    JavaScriptEngine(BrowserCacheManager &cache);
    ~JavaScriptEngine();
    void InjectData();
    std::string Run(const std::string& script_data, const std::string &script_name, bool IsModule);

    bool Step() const;
    void RunAll() const;


    JSContext *create_tab_context(std::string URL, Node *DOM);
    void DispatchEvent(const std::vector<EventListener> &Listeners, const std::string &type);

    void set_active_context(JSContext *ctx, const std::string &url);

    void destroy_tab_context(JSContext *ctx);
    JavaScriptEngine(const JavaScriptEngine&) = delete;
    JavaScriptEngine& operator=(const JavaScriptEngine&) = delete;
private:
QuickjsEngine qjs_engine;
};




#endif //BROWSER_JAVASCRIPTENGINE_H
