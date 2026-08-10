#pragma once
#include "IEngineModule.h"
#include "../QuickjsEngine.h"
#include "Vars/Timers.h"
class DOMBridge : public IEngineModule {
public:
    explicit DOMBridge(QuickjsEngine& engine) : m_engine(engine) {}
    void initialize(JSContext* ctx, const std::string& url, Node *DOM, QuickjsEngine *engine) override;
    std::string name() const override { return "DOMBridge"; }
    static void SetupURL(JSContext *ctx, JSValue global, const std::string &url);

    void tick() override;
private:
    QuickjsEngine& m_engine; // needed for wrap_html_element
    Timers m_timers;
};