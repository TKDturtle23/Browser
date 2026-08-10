#pragma once
#include <string>

#include "Node/Node.h"

class QuickjsEngine;
struct JSContext;

class IEngineModule {
public:
    virtual ~IEngineModule() = default;
    virtual void initialize(JSContext* ctx, const std::string& url, Node *DOM, QuickjsEngine *engine) = 0;
    virtual void tick() = 0;
    virtual std::string name() const = 0;
};