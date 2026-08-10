#include "Timers.h"
#include <algorithm>
#include <iostream>

#include "Debug/Logger.h"

extern "C" {
#include "quickjs.h"
}

// ─── Helpers ────────────────────────────────────────────────────────────────

uint64_t Timers::NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
}

// ─── Core scheduling ────────────────────────────────────────────────────────

int Timers::Schedule(JSContext* ctx, JSValue callback, uint32_t delay_ms, bool repeating) {
    int id = m_next_id++;
    m_timers.push_back({
        .id          = id,
        .fire_at_ms  = NowMs() + delay_ms,
        .interval_ms = delay_ms,
        .repeating   = repeating,
        .cancelled   = false,
        .callback    = JS_DupValue(ctx, callback),  // keep the callback alive
        .ctx         = ctx
    });
    Logger::Log("test", "", 0);
    return id;
}

void Timers::Cancel(int id) {
    for (auto& t : m_timers) {
        if (t.id == id) {
            t.cancelled = true;
            return;
        }
    }
}

// ─── Per-frame tick ─────────────────────────────────────────────────────────

bool  Timers::Tick() {
    uint64_t now = NowMs();
    bool fired = false;
    for (auto& t : m_timers) {
        if (t.cancelled || now < t.fire_at_ms) continue;
        fired = true;
        // Fire the callback
        JSValue ret = JS_Call(t.ctx, t.callback, JS_UNDEFINED, 0, nullptr);
        if (JS_IsException(ret)) {
            // Pull and log the exception without letting it bubble
            JSValue exc = JS_GetException(t.ctx);
            const char* msg = JS_ToCString(t.ctx, exc);
            std::cerr << "[Timer] callback threw: " << (msg ? msg : "unknown") << "\n";
            JS_FreeCString(t.ctx, msg);
            JS_FreeValue(t.ctx, exc);
        }
        JS_FreeValue(t.ctx, ret);

        if (t.repeating)
            t.fire_at_ms = now + t.interval_ms;
        else
            t.cancelled = true;
    }

    // Purge cancelled/expired one-shots
    m_timers.erase(
        std::remove_if(m_timers.begin(), m_timers.end(), [&](const TimerEntry& t) {
            if (t.cancelled) {
                JS_FreeValue(t.ctx, t.callback);
                return true;
            }
            return false;
        }),
        m_timers.end()
    );



    return fired;
}

// ─── Cleanup ────────────────────────────────────────────────────────────────

void Timers::Clear(JSContext* ctx) {
    for (auto& t : m_timers) {
        JS_FreeValue(ctx, t.callback);
    }
    m_timers.clear();
    m_next_id = 1;
}

// ─── JS-callable natives ─────────────────────────────────────────────────────
// Each function receives the Timers* instance via the JS_CFUNC_data mechanism.
// 'data[0]' holds a pointer to the Timers instance encoded as a BigInt.

JSValue Timers::js_set_timeout(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int, JSValue* data) {
    int64_t ptr = 0;
    JS_ToBigInt64(ctx, &ptr, data[0]);
    auto* self = reinterpret_cast<Timers*>(ptr);
    if (!self || argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_NewInt32(ctx, -1);

    uint32_t delay = 0;
    if (argc >= 2) JS_ToUint32(ctx, &delay, argv[1]);

    return JS_NewInt32(ctx, self->Schedule(ctx, argv[0], delay, false));
}

JSValue Timers::js_set_interval(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int, JSValue* data) {
    int64_t ptr = 0;
    JS_ToBigInt64(ctx, &ptr, data[0]);
    auto* self = reinterpret_cast<Timers*>(ptr);
    if (!self || argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_NewInt32(ctx, -1);

    uint32_t delay = 0;
    if (argc >= 2) JS_ToUint32(ctx, &delay, argv[1]);

    return JS_NewInt32(ctx, self->Schedule(ctx, argv[0], delay, true));
}

JSValue Timers::js_clear_timeout(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int, JSValue* data) {
    int64_t ptr = 0;
    JS_ToBigInt64(ctx, &ptr, data[0]);
    auto* self = reinterpret_cast<Timers*>(ptr);
    if (self && argc >= 1) {
        int32_t id = 0;
        JS_ToInt32(ctx, &id, argv[0]);
        self->Cancel(id);
    }
    return JS_UNDEFINED;
}

JSValue Timers::js_clear_interval(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int, JSValue* data) {
    // clearInterval and clearTimeout share the same cancellation logic
    return js_clear_timeout(ctx, JS_UNDEFINED, argc, argv, 0, data);
}

// ─── Registration ────────────────────────────────────────────────────────────

void Timers::SetupTimers(JSContext* ctx, JSValue global) {
    // 'this' pointer encoded as a BigInt so the JS_CFUNC_data callbacks
    // can retrieve it without any global state
    JSValue self_ptr = JS_NewBigInt64(ctx, reinterpret_cast<int64_t>(this));

    auto bind = [&](const char* name, JSCFunctionData* fn, int nargs) {
        JSValue f = JS_NewCFunctionData(ctx, fn, nargs, 0, 1, &self_ptr);
        JS_SetPropertyStr(ctx, global, name, f);
        // f is consumed (owned) by the global object after SetPropertyStr
    };

    bind("setTimeout",    js_set_timeout,    2);
    bind("setInterval",   js_set_interval,   2);
    bind("clearTimeout",  js_clear_timeout,  1);
    bind("clearInterval", js_clear_interval, 1);

    JS_FreeValue(ctx, self_ptr);
}