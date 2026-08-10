#include "DOMUtils.h"
#include "../QuickjsEngine.h"  // QuickjsEngine, get_node_class_id
#include "../../Node/Node.h"    // Node

// ============================================================
//  GetEngine
//
//  The engine pointer is stored as a BigInt on the global
//  object under "__engine_internal_ptr" during DOMBridge::initialize.
//  All C callbacks use this to reach engine state without
//  relying on global/static variables.
// ============================================================
QuickjsEngine *GetEngine(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue ptr    = JS_GetPropertyStr(ctx, global, "__engine_internal_ptr");

    int64_t addr = 0;
    JS_ToBigInt64(ctx, &addr, ptr);

    JS_FreeValue(ctx, ptr);
    JS_FreeValue(ctx, global);

    return reinterpret_cast<QuickjsEngine *>(static_cast<uintptr_t>(addr));
}

Node *FindTag(Node *n, const std::string &tag) {
    if (!n) return nullptr;
    if (n->tag == tag) return n;
    for (auto &child : n->children) {
        if (Node *res = FindTag(child.get(), tag)) return res;
    }
    return nullptr;
}
// ============================================================
//  FindBody
//
//  Pre-order DFS walk. Returns the first node whose tag is
//  "body", or nullptr if none exists in the subtree.
// ============================================================
Node *FindBody(Node *n) {
return FindTag(n, "body");
}

Node * FindHead(Node *root) {
    return FindTag(root, "head");
}
