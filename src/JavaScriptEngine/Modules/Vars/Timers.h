#pragma once
#include <vector>
#include <chrono>

extern "C" {
#include "quickjs.h"
}

struct TimerEntry {
    int      id;
    uint64_t fire_at_ms;   // absolute time to fire
    uint32_t interval_ms;  // 0 = setTimeout, >0 = setInterval
    bool     repeating;
    bool     cancelled;
    JSValue  callback;
    JSContext* ctx;
};

class Timers {
public:
    void SetupTimers(JSContext* ctx, JSValue global);

    // Call once per frame from your render/update loop
    bool Tick();

    // Clears all timers and frees JS callback values
    void Clear(JSContext* ctx);

private:
    std::vector<TimerEntry> m_timers;
    int m_next_id = 1;

    int  Schedule(JSContext* ctx, JSValue callback, uint32_t delay_ms, bool repeating);
    void Cancel(int id);

    static uint64_t NowMs();

    // JS-callable natives — these bounce into the Timers instance via opaque
    static JSValue js_set_timeout    (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic, JSValue* data);
    static JSValue js_set_interval   (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic, JSValue* data);
    static JSValue js_clear_timeout  (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic, JSValue* data);
    static JSValue js_clear_interval (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic, JSValue* data);
};