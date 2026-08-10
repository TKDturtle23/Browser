//
// DOMBridge.cpp
//
// Wires the QuickJS runtime to the browser's DOM tree.
// Responsibilities:
//   - Build the document / window / navigator / location globals
//   - Attach properties, methods, and accessors to each wrapped element
//   - Register event-constructor stubs React (and similar) expect
//   - Install URL / URLSearchParams / Worker / SharedWorker stubs
//

#include "DOMBridge.h"
#include "DOMUtils.h"
#include "../JS_Functions.h"
#include "../QuickjsEngine.h"
#include "DOMStyle.h"
#include <regex>

#include "Vars/TextEncoder.h"

// ============================================================
//  Element setup helpers
//  Each helper focuses on a single concern so they stay small
//  and can be called independently when wrapping new elements.
// ============================================================

static void SetupElementProperties(JSContext *ctx, JSValue js_el) {
    using namespace JavascriptFunctions;

    // magic values match the switch inside js_element_get/set_property
    static const struct { const char *name; int magic; bool writable; } kProps[] = {
        { "tagName",     0, false },
        { "id",          1, true  },
        { "className",   2, true  },
        { "innerHTML",   3, true  },
        { "innerText",   4, true  },
        { "textContent", 4, true  },
        { "nodeName",    0, false },
    };

    for (auto &p : kProps) {
        QuickJS::set_accessor_prop_magic(
            ctx, js_el, p.name, p.magic,
            js_element_get_property,
            p.writable ? js_element_set_property : nullptr);
    }
}

static void SetupElementMethods(JSContext *ctx, JSValue js_el) {
    using namespace JavascriptFunctions;
    using namespace QuickJS;

    set_func_prop(ctx, js_el, "insertBefore",        js_node_insert_before,          2);
    set_func_prop(ctx, js_el, "removeChild",         js_element_removeChild,         1);
    set_func_prop(ctx, js_el, "appendChild",         js_document_appendChild,        1);
    set_func_prop(ctx, js_el, "setAttribute",        js_element_set_attribute,       2);
    set_func_prop(ctx, js_el, "addEventListener",    js_add_event_listener,          3);
    set_func_prop(ctx, js_el, "removeEventListener", js_remove_event_listener,       3);
    set_func_prop(ctx, js_el, "getRootNode",         js_element_get_root_node,       0);
    set_func_prop(ctx, js_el, "setStyleAttribute",   js_element_set_style_attribute, 2);
    set_func_prop(ctx, js_el, "getStyleAttribute",   js_element_get_style_attribute, 0);
}

static void SetupElementAccessors(JSContext *ctx, JSValue js_el, Node *n) {
    using namespace JavascriptFunctions;

    // nodeType: 3 for text nodes, 1 for everything else
    int node_type_magic = (n->type == NodeType::Text) ? 3 : 1;
    QuickJS::set_accessor_prop_magic(ctx, js_el, "nodeType", node_type_magic,
        js_return_node_type, nullptr, JS_PROP_C_W_E);

    // Inline event handler properties
    static const struct { const char *name; int magic; } kEventProps[] = {
        { "oninput",  EVENT_ONINPUT  },
        { "onclick",  EVENT_ONCLICK  },
        { "onchange", EVENT_ONCHANGE },
    };
    for (auto &ep : kEventProps) {
        QuickJS::set_accessor_prop_magic(ctx, js_el, ep.name, ep.magic,
            js_element_get_event_property, js_element_set_event_property);
    }

    QuickJS::set_accessor_prop_magic(ctx, js_el, "ownerDocument", 0,
        js_element_get_owner_document, nullptr, JS_PROP_C_W_E);

    QuickJS::set_accessor_prop_magic(ctx, js_el, "namespaceURI", 0,
        [](JSContext *ctx, JSValueConst, int, JSValueConst *, int) -> JSValue {
            return JS_NewString(ctx, "http://www.w3.org/1999/xhtml");
        },
        nullptr, JS_PROP_C_W_E);

    QuickJS::set_accessor_prop_magic(ctx, js_el, "firstChild", 0,
        [](JSContext *ctx, JSValueConst this_val, int, JSValueConst *, int) -> JSValue {
            auto *engine = GetEngine(ctx);
            auto *node   = static_cast<Node *>(
                JS_GetOpaque(this_val, QuickjsEngine::get_node_class_id()));
            if (!node || node->children.empty()) return JS_NULL;
            return engine->wrap_html_element(node->children[0].get());
        },
        nullptr, JS_PROP_C_W_E);

    // <script> elements expose src as a readable property
    if (n->tag == "script") {
        QuickJS::set_accessor_prop_magic(ctx, js_el, "src", 1,
            js_element_script_get_property,
            js_element_script_get_property);
    }

    // dispatchEvent — no-op stub; returns true ("not cancelled")
    QuickJS::set_func_prop(ctx, js_el, "dispatchEvent",
        [](JSContext *, JSValueConst, int, JSValueConst *) -> JSValue {
            return JS_TRUE;
        }, 1);
}

// Public entry-point called by the element-wrapper callback
void SetupHTMLElement(JSContext *ctx, JSValue js_el, Node *n,
                      QuickjsEngine *engine, JSValue html_element_proto) {
    SetupElementProperties(ctx, js_el);
    SetupElementMethods(ctx, js_el);
    SetupElementAccessors(ctx, js_el, n);
    SetupStyleObject(ctx, js_el, n);
    JS_SetPrototype(ctx, js_el, html_element_proto);
}

// ============================================================
//  Global object factories
//  Each returns a fully-formed JSValue; callers own the ref.
// ============================================================

static JSValue MakeLocationObject(JSContext *ctx, const std::string &url) {
    static const std::regex kUrlRe(
        R"(^(https?):\/\/([^/:]+)(?::(\d+))?(\/[^?#]*)?(\?[^#]*)?(#.*)?)");

    std::smatch m;
    std::string protocol, hostname, port, pathname, search, hash;

    if (std::regex_match(url, m, kUrlRe)) {
        protocol = m[1].str() + ":";
        hostname = m[2].str();
        port     = m[3].str();
        pathname = m[4].matched ? m[4].str() : "/";
        search   = m[5].matched ? m[5].str() : "";
        hash     = m[6].matched ? m[6].str() : "";
    }

    const std::string host   = port.empty() ? hostname : hostname + ":" + port;
    const std::string origin = protocol + "//" + host;

    JSValue loc = JS_NewObject(ctx);
    QuickJS::set_string_prop(ctx, loc, "hostname", hostname);
    QuickJS::set_string_prop(ctx, loc, "port",     port);
    QuickJS::set_string_prop(ctx, loc, "protocol", protocol);
    QuickJS::set_string_prop(ctx, loc, "pathname", pathname);
    QuickJS::set_string_prop(ctx, loc, "search",   search);
    QuickJS::set_string_prop(ctx, loc, "hash",     hash);
    QuickJS::set_string_prop(ctx, loc, "host",     host);
    QuickJS::set_string_prop(ctx, loc, "origin",   origin);
    QuickJS::set_string_prop(ctx, loc, "href",     url);
    return loc;
}

static JSValue MakeNavigatorObject(JSContext *ctx) {
    JSValue nav = JS_NewObject(ctx);

    QuickJS::set_string_prop(ctx, nav, "userAgent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Browser/1.0");
    QuickJS::set_string_prop(ctx, nav, "platform", "Win32");
    QuickJS::set_string_prop(ctx, nav, "language", "en-US");
    QuickJS::set_string_prop(ctx, nav, "vendor",   "Euclase");
    QuickJS::set_string_prop(ctx, nav, "product",  "Gecko");

    JSValue languages = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, languages, 0, JS_NewString(ctx, "en-US"));
    JS_SetPropertyUint32(ctx, languages, 1, JS_NewString(ctx, "en"));
    JS_SetPropertyStr(ctx, nav, "languages", languages);

    JS_SetPropertyStr(ctx, nav, "hardwareConcurrency", JS_NewInt32(ctx, 8));
    JS_SetPropertyStr(ctx, nav, "maxTouchPoints",      JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, nav, "onLine",              JS_NewBool(ctx, true));
    JS_SetPropertyStr(ctx, nav, "cookieEnabled",       JS_NewBool(ctx, false));

    // userAgentData — minimal surface for client-hints checks
    JSValue ua_data = JS_NewObject(ctx);
    QuickJS::set_string_prop(ctx, ua_data, "platform", "Windows");
    JS_SetPropertyStr(ctx, ua_data, "mobile", JS_NewBool(ctx, false));
    JSValue brands = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, brands, 0, JS_NewString(ctx, "Chromium"));
    JS_SetPropertyStr(ctx, ua_data, "brands", brands);
    JS_SetPropertyStr(ctx, nav, "userAgentData", ua_data);

    return nav;
}

static JSValue MakeDocumentObject(JSContext *ctx, Node *DOM, QuickjsEngine &engine) {
    using namespace JavascriptFunctions;
    using namespace QuickJS;

    JSValue doc = JS_NewObjectClass(ctx, QuickjsEngine::get_node_class_id());

    // Core DOM methods
    set_func_prop(ctx, doc, "addEventListener",     js_add_event_listener,                3);
    set_func_prop(ctx, doc, "removeEventListener",  js_remove_event_listener,             3);
    set_func_prop(ctx, doc, "getElementById",       js_document_get_element_by_id,        1);
    set_func_prop(ctx, doc, "getElementsByTagName", js_document_get_elements_by_tag_name, 1);
    set_func_prop(ctx, doc, "createElement",        js_document_create_element,           1);
    set_func_prop(ctx, doc, "createTextNode",       js_document_createTextNode,           1);
    set_func_prop(ctx, doc, "querySelector",        js_document_query_selector,           1);
    set_func_prop(ctx, doc, "querySelectorAll",     js_document_query_selector_all,       1);
    set_func_prop(ctx, doc, "getRootNode",          js_element_get_root_node,             0);

    // Data accessors
    set_accessor_prop(ctx, doc, "title", 0, js_document_get_data, js_document_set_data);

    set_accessor_prop_magic(ctx, doc, "activeElement", 0,
        js_document_get_active_element, nullptr, JS_PROP_C_W_E);

    // Identity
    set_string_prop(ctx, doc, "nodeName", "#document");
    JS_SetPropertyStr(ctx, doc, "nodeType", JS_NewInt32(ctx, 9));

    // body shortcut and opaque DOM pointer
    Node *body = FindBody(DOM);
    JS_SetPropertyStr(ctx, doc, "body", engine.wrap_html_element(body));

    Node *head = FindHead(DOM);
    JS_SetPropertyStr(ctx, doc, "head", engine.wrap_html_element(head));

    JS_SetOpaque(doc, DOM);

    return doc;
}

// ============================================================
//  Stub constructors
//  Grouped so the large DOMBridge::initialize stays readable.
// ============================================================

// Shared URL-parsing regex; used by both MakeLocationObject and SetupURL
static bool ParseURL(const std::string &url,
                     std::string &protocol, std::string &hostname, std::string &port,
                     std::string &pathname, std::string &search,  std::string &hash) {
    static const std::regex kRe(
        R"(^(https?):\/\/([^/:]+)(?::(\d+))?(\/[^?#]*)?(\?[^#]*)?(#.*)?)");
    std::smatch m;
    if (!std::regex_match(url, m, kRe)) return false;
    protocol = m[1].str() + ":";
    hostname = m[2].str();
    port     = m[3].str();
    pathname = m[4].matched ? m[4].str() : "/";
    search   = m[5].matched ? m[5].str() : "";
    hash     = m[6].matched ? m[6].str() : "";
    return true;
}

static void AttachURLProperties(JSContext *ctx, JSValue obj, const std::string &full) {
    std::string protocol, hostname, port, pathname, search, hash;
    ParseURL(full, protocol, hostname, port, pathname, search, hash);

    const std::string host   = port.empty() ? hostname : hostname + ":" + port;
    const std::string origin = protocol + "//" + host;

    QuickJS::set_string_prop(ctx, obj, "href",     full);
    QuickJS::set_string_prop(ctx, obj, "protocol", protocol);
    QuickJS::set_string_prop(ctx, obj, "hostname", hostname);
    QuickJS::set_string_prop(ctx, obj, "port",     port);
    QuickJS::set_string_prop(ctx, obj, "pathname", pathname);
    QuickJS::set_string_prop(ctx, obj, "search",   search);
    QuickJS::set_string_prop(ctx, obj, "hash",     hash);
    QuickJS::set_string_prop(ctx, obj, "host",     host);
    QuickJS::set_string_prop(ctx, obj, "origin",   origin);
}

void DOMBridge::SetupURL(JSContext *ctx, JSValue global, const std::string &url) {
    // --- URL constructor ---
    JSValue url_ctor = JS_NewCFunction2(ctx,
        [](JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) -> JSValue {
            JSValue obj = JS_NewObject(ctx);

            std::string href, base;
            if (argc >= 1) {
                const char *s = JS_ToCString(ctx, argv[0]);
                if (s) { href = s; JS_FreeCString(ctx, s); }
            }
            if (argc >= 2) {
                const char *s = JS_ToCString(ctx, argv[1]);
                if (s) { base = s; JS_FreeCString(ctx, s); }
            }

            // Resolve a relative href against the base origin
            std::string full = href;
            if (!base.empty() && (href.empty() || href[0] == '/')) {
                auto pos = base.find("://");
                if (pos != std::string::npos) {
                    auto slash  = base.find('/', pos + 3);
                    std::string origin = (slash != std::string::npos)
                        ? base.substr(0, slash) : base;
                    full = origin + (href.empty() ? "/" : href);
                }
            }

            AttachURLProperties(ctx, obj, full);

            // Minimal searchParams stub
            JSValue sp = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, sp, "get",
                JS_NewCFunction(ctx, [](JSContext *, JSValueConst, int, JSValueConst *) {
                    return JS_NULL;
                }, "get", 1));
            JS_SetPropertyStr(ctx, obj, "searchParams", sp);

            // toString() so template literals resolve to href
            JS_SetPropertyStr(ctx, obj, "toString",
                JS_NewCFunction(ctx, [](JSContext *ctx, JSValueConst this_val, int, JSValueConst *) {
                    return JS_GetPropertyStr(ctx, this_val, "href");
                }, "toString", 0));

            return obj;
        }, "URL", 1, JS_CFUNC_constructor, 0);

    JS_SetPropertyStr(ctx, url_ctor, "prototype", JS_NewObject(ctx));
    JS_SetPropertyStr(ctx, global,   "URL",       url_ctor);

    // --- URLSearchParams constructor ---
    JSValue usp_ctor = JS_NewCFunction2(ctx,
        [](JSContext *ctx, JSValueConst, int, JSValueConst *) -> JSValue {
            JSValue obj = JS_NewObject(ctx);
            auto noop_null = [](JSContext *, JSValueConst, int, JSValueConst *) {
                return JS_NULL;
            };
            auto noop_undef = [](JSContext *, JSValueConst, int, JSValueConst *) {
                return JS_UNDEFINED;
            };
            auto noop_false = [](JSContext *, JSValueConst, int, JSValueConst *) {
                return JS_FALSE;
            };
            JS_SetPropertyStr(ctx, obj, "get",      JS_NewCFunction(ctx, noop_null,  "get",      1));
            JS_SetPropertyStr(ctx, obj, "set",      JS_NewCFunction(ctx, noop_undef, "set",      2));
            JS_SetPropertyStr(ctx, obj, "has",      JS_NewCFunction(ctx, noop_false, "has",      1));
            JS_SetPropertyStr(ctx, obj, "toString",
                JS_NewCFunction(ctx, [](JSContext *ctx, JSValueConst, int, JSValueConst *) {
                    return JS_NewString(ctx, "");
                }, "toString", 0));
            return obj;
        }, "URLSearchParams", 0, JS_CFUNC_constructor, 0);

    JS_SetPropertyStr(ctx, usp_ctor, "prototype", JS_NewObject(ctx));
    JS_SetPropertyStr(ctx, global,   "URLSearchParams", usp_ctor);
}

void DOMBridge::tick() {
m_timers.Tick();
}

static void SetupWorkerStubs(JSContext *ctx, JSValue global) {
    // No-op helper shared by both worker types
    static const auto noop = [](JSContext *, JSValueConst, int, JSValueConst *) -> JSValue {
        return JS_UNDEFINED;
    };

    // --- SharedWorker ---
    JSValue sw_ctor = JS_NewCFunction2(ctx,
        [](JSContext *ctx, JSValueConst, int, JSValueConst *) -> JSValue {
            JSValue port = JS_NewObject(ctx);
            const auto n = [](JSContext *, JSValueConst, int, JSValueConst *) -> JSValue {
                return JS_UNDEFINED;
            };
            JS_SetPropertyStr(ctx, port, "addEventListener",    JS_NewCFunction(ctx, n, "addEventListener",    3));
            JS_SetPropertyStr(ctx, port, "removeEventListener", JS_NewCFunction(ctx, n, "removeEventListener", 3));
            JS_SetPropertyStr(ctx, port, "postMessage",         JS_NewCFunction(ctx, n, "postMessage",         1));
            JS_SetPropertyStr(ctx, port, "start",               JS_NewCFunction(ctx, n, "start",               0));
            JS_SetPropertyStr(ctx, port, "close",               JS_NewCFunction(ctx, n, "close",               0));

            JSValue obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, obj, "port", port);
            return obj;
        }, "SharedWorker", 1, JS_CFUNC_constructor, 0);

    JS_SetPropertyStr(ctx, sw_ctor, "prototype", JS_NewObject(ctx));
    JS_SetPropertyStr(ctx, global,  "SharedWorker", sw_ctor);

    // --- Worker ---
    JSValue worker_ctor = JS_NewCFunction2(ctx,
        [](JSContext *ctx, JSValueConst, int, JSValueConst *) -> JSValue {
            const auto n = [](JSContext *, JSValueConst, int, JSValueConst *) -> JSValue {
                return JS_UNDEFINED;
            };
            JSValue obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, obj, "postMessage",         JS_NewCFunction(ctx, n, "postMessage",         1));
            JS_SetPropertyStr(ctx, obj, "terminate",           JS_NewCFunction(ctx, n, "terminate",           0));
            JS_SetPropertyStr(ctx, obj, "addEventListener",    JS_NewCFunction(ctx, n, "addEventListener",    3));
            JS_SetPropertyStr(ctx, obj, "removeEventListener", JS_NewCFunction(ctx, n, "removeEventListener", 3));
            return obj;
        }, "Worker", 1, JS_CFUNC_constructor, 0);

    JS_SetPropertyStr(ctx, worker_ctor, "prototype", JS_NewObject(ctx));
    JS_SetPropertyStr(ctx, global,      "Worker",    worker_ctor);
}

static void SetupEventConstructors(JSContext *ctx, JSValue global) {
    static const char *kNames[] = {
        "EventTarget", "Event", "CustomEvent", "MouseEvent", "KeyboardEvent",
        "AnimationEvent", "TransitionEvent", "ErrorEvent", "MessageEvent",
        "FocusEvent", "InputEvent", "PointerEvent", "WheelEvent", nullptr
    };

    for (const char **name = kNames; *name; ++name) {
        JSValue ctor = JS_NewCFunction2(ctx,
            [](JSContext *ctx, JSValueConst, int, JSValueConst *) -> JSValue {
                return JS_NewObject(ctx);
            }, *name, 0, JS_CFUNC_constructor, 0);

        JSValue proto = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, proto, "bubbles",    JS_NewBool(ctx, false));
        JS_SetPropertyStr(ctx, proto, "cancelable", JS_NewBool(ctx, false));
        JS_SetPropertyStr(ctx, proto, "composed",   JS_NewBool(ctx, false));
        JS_SetPropertyStr(ctx, proto, "stopPropagation",
            JS_NewCFunction(ctx, [](JSContext *, JSValueConst, int, JSValueConst *) {
                return JS_UNDEFINED;
            }, "stopPropagation", 0));
        JS_SetPropertyStr(ctx, proto, "preventDefault",
            JS_NewCFunction(ctx, [](JSContext *, JSValueConst, int, JSValueConst *) {
                return JS_UNDEFINED;
            }, "preventDefault", 0));

        JS_SetPropertyStr(ctx, ctor,   "prototype", proto);
        JS_SetPropertyStr(ctx, global, *name,       ctor);
    }
}

// ============================================================
//  DOMBridge::initialize
// ============================================================

void DOMBridge::initialize(JSContext *ctx, const std::string &url,
                           Node *DOM, QuickjsEngine *engine) {
    JSValue global = JS_GetGlobalObject(ctx);

    // Store engine pointer so C callbacks can recover it via GetEngine()
    JS_SetPropertyStr(ctx, global, "__engine_internal_ptr",
        JS_NewBigInt64(ctx, reinterpret_cast<int64_t>(&m_engine)));

    // Sentinel for the active event (set/cleared around dispatch)
    JS_SetPropertyStr(ctx, global, "event", JS_UNDEFINED);

    // HTMLElement base class — elements get this as their prototype
    JSValue html_element_ctor  = JS_NewCFunction2(ctx,
        [](JSContext *ctx, JSValueConst, int, JSValueConst *) -> JSValue {
            return JS_NewObject(ctx);
        }, "HTMLElement", 0, JS_CFUNC_constructor, 0);
    JSValue html_element_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, html_element_ctor, "prototype", html_element_proto);
    JS_SetPropertyStr(ctx, global, "HTMLElement", html_element_ctor);

    // HTMLIFrameElement (inherits HTMLElement)
    JSValue iframe_ctor  = JS_NewCFunction2(ctx,
        [](JSContext *ctx, JSValueConst, int, JSValueConst *) -> JSValue {
            return JS_NewObject(ctx);
        }, "HTMLIFrameElement", 0, JS_CFUNC_constructor, 0);
    JSValue iframe_proto = JS_NewObject(ctx);
    JS_SetPrototype(ctx, iframe_proto, html_element_proto);
    JS_SetPropertyStr(ctx, iframe_ctor, "prototype", iframe_proto);
    JS_SetPropertyStr(ctx, global, "HTMLIFrameElement", iframe_ctor);

    // Global dispatchEvent — no-op stub, returns true ("not cancelled")
    QuickJS::set_func_prop(ctx, global, "dispatchEvent",
        [](JSContext *, JSValueConst, int, JSValueConst *) -> JSValue {
            return JS_TRUE;
        }, 1);

    SetupEventConstructors(ctx, global);

    // Global event listener forwarding
    QuickJS::set_func_prop(ctx, global, "addEventListener",
        JavascriptFunctions::js_add_event_listener, 2);
    QuickJS::set_func_prop(ctx, global, "removeEventListener",
        JavascriptFunctions::js_remove_event_listener, 2);

    // Core globals
    JS_SetPropertyStr(ctx, global, "location",  MakeLocationObject(ctx, url));
    JS_SetPropertyStr(ctx, global, "navigator", MakeNavigatorObject(ctx));

    JSValue doc = MakeDocumentObject(ctx, DOM, m_engine);
    JS_SetPropertyStr(ctx, doc,    "defaultView", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, global, "document",    doc);


    // Register the element-wrapper callback so wrap_html_element() sets up
    // all properties/methods/accessors for freshly created nodes.
    m_engine.set_element_wrapper(
        [this, html_element_proto](JSContext *ctx, JSValue js_el, Node *node) {
            SetupHTMLElement(ctx, js_el, node, &m_engine, html_element_proto);
        });

    SetupURL(ctx, global, url);
    SetupWorkerStubs(ctx, global);
    TextEncoder::SetupTextEncoder(ctx, global);

    m_timers.SetupTimers(ctx, global);



    // window / self aliases — must come after all other globals are populated
    JS_SetPropertyStr(ctx, global, "window", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, global, "self",   JS_DupValue(ctx, global));

    JS_FreeValue(ctx, global);
}