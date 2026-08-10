//
// Created by tkdtu on 6/15/2026.
//

#ifndef BROWSER_CONSOLEBRIDGE_H
#define BROWSER_CONSOLEBRIDGE_H
#include "IEngineModule.h"
#include "../QuickjsEngine.h"

class  ConsoleBridge : public IEngineModule {
public:
    explicit ConsoleBridge() {}
    void initialize(JSContext* ctx, const std::string& url, Node *DOM, QuickjsEngine *engine) override;
    std::string name() const override { return "ConsoleBridge"; }
    void tick() override;
private:
};


#endif //BROWSER_CONSOLEBRIDGE_H
