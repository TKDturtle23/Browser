// bootloader.js
const MONITOR_CONFIG = {
    enabled: true,
    logGet: true,
    logSet: true,
    logHas: true,
    monitorWindow: true,
    monitorDocument: true,
    monitorElements: true,
    sentinelUndefined: true,   // the undefined->function sentinel
    ignoreKeys: new Set(['__reactFiber', '__reactProps',
        'then', 'constructor', 'prototype', '__esModule',
    ]),
    ignorePrefixes: ['__react', '_react', '__proto'],
    filterPattern: null,
};
const PROXY_CONFIG = {
    wrapWindow: true,
    wrapDocument: true,
    wrapElements: true,
};
function shouldLog(key) {
    if (typeof key === 'symbol') return false;
    if (MONITOR_CONFIG.ignoreKeys.has(key)) return false;
    if (MONITOR_CONFIG.ignorePrefixes.some(p => key.startsWith(p))) return false;
    if (MONITOR_CONFIG.filterPattern && !MONITOR_CONFIG.filterPattern.test(key)) return false;
    return true;
}

// Tracks created elements so we can label them in logs
let _elementCounter = 0;

function makeMonitorProxy(target, label) {
    return new Proxy(target, {
        get(obj, key, receiver) {
            const val = Reflect.get(obj, key);
            if (MONITOR_CONFIG.enabled && MONITOR_CONFIG.logGet && shouldLog(key)) {
                const type = typeof val;
                const preview = type === 'function'  ? '[Function]'
                    : type === 'undefined' ? 'undefined'
                        : type === 'object'    ? (val === null ? 'null' : '[Object]')
                            : String(val);
                print(`[MONITOR] ${label}.${key} GET => ${preview}`);
            }
            // Bind native functions to the real object so JS_GetOpaque works
            if (typeof val === 'function') {
                return val.bind(obj);
            }

            return val;
        },
        set(obj, key, value, receiver) {
            if (MONITOR_CONFIG.enabled && MONITOR_CONFIG.logSet && shouldLog(key)) {
                const preview = typeof value === 'function' ? '[Function]'
                    : typeof value === 'object'   ? (value === null ? 'null' : '[Object]')
                        : String(value);
                print(`[MONITOR] ${label}.${key} SET <= ${preview}`);
            }
            return Reflect.set(obj, key, value, receiver);
        },
        has(obj, key) {
            const result = Reflect.has(obj, key);
            if (MONITOR_CONFIG.enabled && shouldLog(key)) {
                print(`[MONITOR] ${label}.${key} IN => ${result}`);
                if (result) {
                    // Log what we'd actually GET when in returns true
                    try {
                        const val = Reflect.get(obj, key);
                        print(`[MONITOR]   (value when true: typeof=${typeof val})`);
                        if (typeof val === 'object' || typeof val === 'function') {
                            try {
                                const proto = val.prototype;
                                print(`[MONITOR]   (.prototype typeof=${typeof proto}, val=${proto})`);
                                if (proto !== null && proto !== undefined) {
                                    print(`[MONITOR]   (.prototype keys=${Object.getOwnPropertyNames(proto).join(', ')})`);
                                }
                            } catch(e) {
                                print(`[MONITOR]   (.prototype access threw: ${e})`);
                            }
                        }
                    } catch(e) {
                        print(`[MONITOR]   (get threw: ${e})`);
                    }
                }
            }
            return result;
        },
    });
}

function makeElementProxy(nativeEl, tag) {
    const id = _elementCounter++;
    const label = `<${tag}#${id}>`;

    print(`[MONITOR] makeElementProxy entered: ${label}`);
    print(`[MONITOR] creating Proxy for ${label}`);

    const p = new Proxy(nativeEl, {
        get(obj, key, receiver) {
            // FIX: Remove 'receiver' so 'this' remains the native element
            const val = Reflect.get(obj, key);

            if (MONITOR_CONFIG.enabled && MONITOR_CONFIG.logGet && shouldLog(key)) {
                const type = typeof val;
                const preview = type === 'function'  ? '[Function]'
                    : type === 'undefined' ? 'undefined'
                        : type === 'object'    ? (val === null ? 'null' : '[Object]')
                            : String(val);
                print(`[MONITOR] ${label}.${key} GET => ${preview}`);
            }

            // Bind native functions to the real element
            if (typeof val === 'function') {
                return val.bind(obj);
            }

            if (val === undefined && typeof key === 'string') {
                const forbiddenSentinels = new Set(['tagName', 'nodeName', 'namespaceURI', 'ownerDocument', 'attributes']);
                if (forbiddenSentinels.has(key)) {
                    return val;
                }
                // Return your sentinel function...
                return new Proxy(function(){}, {
                    apply(_t, _this, args) {
                        print(`[MONITOR] CALLED UNDEFINED METHOD: ${label}.${key}(${args.map(String).join(', ').slice(0,60)})`);
                        return undefined;
                    },
                    get(_t, p) {
                        if (p === 'call' || p === 'apply' || p === 'bind') return Function.prototype[p];
                        return undefined;
                    }
                });
            }
            return val;
        },
        set(obj, key, value, receiver) {
            if (MONITOR_CONFIG.enabled && MONITOR_CONFIG.logSet && shouldLog(key)) {
                const preview = typeof value === 'function' ? '[Function]'
                    : typeof value === 'object'   ? (value === null ? 'null' : '[Object]')
                        : String(value);
                print(`[MONITOR] ${label}.${key} SET <= ${preview}`);
            }
            return Reflect.set(obj, key, value);;
        },
        has(obj, key) {
            const result = Reflect.has(obj, key);
            if (MONITOR_CONFIG.enabled && shouldLog(key)) {
                print(`[MONITOR] ${label}.${key} IN => ${result}`);
                if (result) {
                    // Log what we'd actually GET when in returns true
                    try {
                        const val = Reflect.get(obj, key);
                        print(`[MONITOR]   (value when true: typeof=${typeof val})`);
                        if (typeof val === 'object' || typeof val === 'function') {
                            try {
                                const proto = val.prototype;
                                print(`[MONITOR]   (.prototype typeof=${typeof proto}, val=${proto})`);
                                if (proto !== null && proto !== undefined) {
                                    print(`[MONITOR]   (.prototype keys=${Object.getOwnPropertyNames(proto).join(', ')})`);
                                }
                            } catch(e) {
                                print(`[MONITOR]   (.prototype access threw: ${e})`);
                            }
                        }
                    } catch(e) {
                        print(`[MONITOR]   (get threw: ${e})`);
                    }
                }
            }
            return result;
        },
    });
    print(`[MONITOR] Proxy created for ${label}`);
    return p;
}

const _nativeDocument = globalThis.document;
const originalCreateElement  = _nativeDocument.createElement.bind(_nativeDocument);
const originalGetElementById = _nativeDocument.getElementById.bind(_nativeDocument);

globalThis.window   = PROXY_CONFIG.wrapWindow   ? makeMonitorProxy(globalThis.window,   'window')   : globalThis.window;
globalThis.document = PROXY_CONFIG.wrapDocument ? makeMonitorProxy(globalThis.document, 'document') : globalThis.document;
// patchElementStyle is referenced here but defined below — hoist it
// by assigning through a wrapper that calls it late

document.createElement = function(tagName) {
    let el;
    try { el = originalCreateElement(tagName); }
    catch(e) { print(`[MONITOR] createElement THREW: ${e}`); return null; }
    return PROXY_CONFIG.wrapElements ? makeElementProxy(el, tagName) : el;
};

document.getElementById = function(id) {
    const el = originalGetElementById(id);
    if (el === null || el === undefined) return el;
    return PROXY_CONFIG.wrapElements ? makeElementProxy(el, el.tagName || 'unknown') : el;
};
// ==== END ACCESS MONITOR ====

import * as nativeOs from 'os';
const _missingEventStub = Object.freeze({
    prototype: Object.freeze({
        bubbles: false,
        cancelable: false,
        composed: false,
        stopPropagation() {},
        preventDefault() {},
    })
});

const _eventConstructorNames = [
    'AnimationEvent', 'TransitionEvent', 'FocusEvent', 'InputEvent',
    'PointerEvent', 'TouchEvent', 'WheelEvent', 'DragEvent',
    'ClipboardEvent', 'BeforeUnloadEvent', 'HashChangeEvent',
    'PopStateEvent', 'ErrorEvent', 'ProgressEvent', 'MessageEvent',
    'StorageEvent', 'PageTransitionEvent', 'UIEvent', 'DeviceMotionEvent',
    'DeviceOrientationEvent', 'GamepadEvent', 'SecurityPolicyViolationEvent',
    'SpeechSynthesisEvent', 'TrackEvent', 'VRDisplayEvent',
];
for (const name of _eventConstructorNames) {
    if (!globalThis[name] || typeof globalThis[name].prototype === 'undefined') {
        function EventStub(type, init) {
            this.type = type;
            this.bubbles = (init && init.bubbles) || false;
            this.cancelable = (init && init.cancelable) || false;
        }
        EventStub.prototype = _missingEventStub.prototype;
        Object.defineProperty(EventStub, 'name', { value: name });
        globalThis[name] = EventStub;
    }
}
globalThis.setTimeout  = nativeOs.setTimeout;
globalThis.setInterval = nativeOs.setInterval;
globalThis.clearTimeout = nativeOs.clearTimeout;
// Assets/BootloaderScript.js

window.top  = window;
window.self = window;

window[Symbol.toPrimitive] = function(hint) {
    return hint === "string" ? "[object Window]" : null;
};
Object.defineProperty(window, Symbol.toStringTag, { value: 'Window', configurable: true, writable: true });
Object.defineProperty(document, Symbol.toStringTag, { value: 'HTMLDocument', configurable: true, writable: true });

if (!window.toString)   window.toString   = () => "[object Window]";
if (!document.toString) document.toString = () => "[object HTMLDocument]";


window[Symbol.toPrimitive] = (hint) => hint === "string" ? "[object Window]" : null;
Object.defineProperty(window, Symbol.toStringTag, { value: 'Window', configurable: true });
Object.defineProperty(document, Symbol.toStringTag, { value: 'HTMLDocument', configurable: true });
