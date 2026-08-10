#include "DOMStyle.h"

#include "JavaScriptEngine/QuickjsEngine.h"


// ============================================================================
// 1. Helper Function: DRY up the Node extraction logic
// ============================================================================
static Node* GetNodeFromStyle(JSContext *ctx, JSValueConst style_obj) {
    JSValue el_val = JS_GetPropertyStr(ctx, style_obj, "__el");
    if (JS_IsUndefined(el_val)) return nullptr;

    auto *node = static_cast<Node *>(JS_GetOpaque(el_val, QuickjsEngine::get_node_class_id()));
    JS_FreeValue(ctx, el_val);
    return node;
}

// ============================================================================
// 2. Standard CSSStyleDeclaration Methods
// ============================================================================
static JSValue Style_setProperty(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_UNDEFINED;
    Node *node = GetNodeFromStyle(ctx, this_val);
    if (!node) return JS_UNDEFINED;

    const char *name = JS_ToCString(ctx, argv[0]);
    const char *value = JS_ToCString(ctx, argv[1]);

    if (name && value) {
        node->style_properties[name] = value;
    }

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

static JSValue Style_getPropertyValue(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NewString(ctx, "");
    Node *node = GetNodeFromStyle(ctx, this_val);
    if (!node) return JS_NewString(ctx, "");

    const char *name = JS_ToCString(ctx, argv[0]);
    std::string result = node->style_properties[name];
    JS_FreeCString(ctx, name);

    return JS_NewString(ctx, result.c_str());
}

static JSValue Style_removeProperty(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    Node *node = GetNodeFromStyle(ctx, this_val);
    if (!node || argc < 1) return JS_NewString(ctx, "");

    const char *name = JS_ToCString(ctx, argv[0]);
    std::string removed_val = node->style_properties[name];
    node->style_properties.erase(name); // Assuming std::map or std::unordered_map
    JS_FreeCString(ctx, name);

    return JS_NewString(ctx, removed_val.c_str());
}

static JSValue Style_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    // Basic stub. Real implementation would iterate the map by index.
    return JS_NewString(ctx, "");
}

// ============================================================================
// 3. Magic Getters/Setters for Direct Property Access (e.g. style.color)
// ============================================================================
enum StylePropMagic {
    PROP_COLOR,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_DISPLAY,
    PROP_BACKGROUND_COLOR
};

static const char* GetStylePropName(int magic) {
    switch(magic) {
        case PROP_COLOR: return "color";
        case PROP_WIDTH: return "width";
        case PROP_HEIGHT: return "height";
        case PROP_DISPLAY: return "display";
        case PROP_BACKGROUND_COLOR: return "background-color";
        default: return "";
    }
}

static JSValue Style_getProp(JSContext *ctx, JSValueConst this_val, int magic) {
    Node *node = GetNodeFromStyle(ctx, this_val);
    if (!node) return JS_UNDEFINED;

    std::string result = node->style_properties[GetStylePropName(magic)];
    return JS_NewString(ctx, result.c_str());
}

static JSValue Style_setProp(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic) {
    Node *node = GetNodeFromStyle(ctx, this_val);
    if (!node) return JS_UNDEFINED;

    const char *value_str = JS_ToCString(ctx, val);
    if (value_str) {
        node->style_properties[GetStylePropName(magic)] = value_str;
        JS_FreeCString(ctx, value_str);
    }
    return JS_UNDEFINED;
}

// ============================================================================
// 4. QuickJS Function List Definition
// ============================================================================
static const JSCFunctionListEntry style_funcs[] = {
    // Methods
    JS_CFUNC_DEF("setProperty", 2, Style_setProperty),
    JS_CFUNC_DEF("getPropertyValue", 1, Style_getPropertyValue),
    JS_CFUNC_DEF("removeProperty", 1, Style_removeProperty),
    JS_CFUNC_DEF("item", 1, Style_item),

    // Direct Properties (Allows: element.style.color = "red")
    JS_CGETSET_MAGIC_DEF("color", Style_getProp, Style_setProp, PROP_COLOR),
    JS_CGETSET_MAGIC_DEF("width", Style_getProp, Style_setProp, PROP_WIDTH),
    JS_CGETSET_MAGIC_DEF("height", Style_getProp, Style_setProp, PROP_HEIGHT),
    JS_CGETSET_MAGIC_DEF("display", Style_getProp, Style_setProp, PROP_DISPLAY),
    JS_CGETSET_MAGIC_DEF("backgroundColor", Style_getProp, Style_setProp, PROP_BACKGROUND_COLOR),
};

// ============================================================================
// 5. The Setup Function
// ============================================================================
 void SetupStyleObject(JSContext *ctx, JSValue js_el, Node *n) {
    JSValue style_obj = JS_NewObject(ctx);

    // Keep reference to element
    JS_SetPropertyStr(ctx, style_obj, "__el", JS_DupValue(ctx, js_el));

    // Bind all functions and getters/setters in one clean pass
    JS_SetPropertyFunctionList(ctx, style_obj, style_funcs, sizeof(style_funcs) / sizeof(JSCFunctionListEntry));

    // Simple static properties
    JS_SetPropertyStr(ctx, style_obj, "cssText", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, style_obj, "length",  JS_NewInt32(ctx, 0));

    // Attach to the main element
    JS_SetPropertyStr(ctx, js_el, "style", style_obj);
}