//
// Created by tkdtu on 6/24/2026.
//

#include "TextEncoder.h"

#include "../Domutils.h"

static JSClassID js_text_encoder_class_id = 0;

static size_t utf8_safe_boundary(const char *utf8, size_t max_bytes)
{
    while (max_bytes > 0 && (utf8[max_bytes] & 0xC0) == 0x80)
        --max_bytes;
    return max_bytes;
}

static uint32_t utf8_bytes_to_utf16_len(const char *utf8, size_t byte_count)
{
    uint32_t code_units = 0;
    size_t i = 0;
    while (i < byte_count) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        size_t seq_len;
        if      (c < 0x80) seq_len = 1;
        else if (c < 0xE0) seq_len = 2;
        else if (c < 0xF0) seq_len = 3;
        else               seq_len = 4;
        code_units += (seq_len == 4) ? 2 : 1;
        i += seq_len;
    }
    return code_units;
}

// ---------------------------------------------------------------------------
// Constructor: new TextEncoder()
// ---------------------------------------------------------------------------
static JSValue js_text_encoder_ctor(JSContext *ctx,
                                    JSValueConst new_target,
                                    int argc,
                                    JSValueConst *argv)
{
    // new_target is undefined when called without `new`
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "TextEncoder must be called with new");

    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        return proto;

    JSValue obj = JS_NewObjectProtoClass(ctx, proto, js_text_encoder_class_id);
    JS_FreeValue(ctx, proto);

    if (JS_IsException(obj))
        return obj;

    return obj;
}

// ---------------------------------------------------------------------------
// TextEncoder.prototype.encode
// ---------------------------------------------------------------------------
static JSValue js_encoder_encode(JSContext *ctx,
                                 JSValueConst this_val,
                                 int argc,
                                 JSValueConst *argv)
{
    JSValue input = (argc < 1 || JS_IsUndefined(argv[0]))
                    ? JS_NewString(ctx, "")
                    : JS_DupValue(ctx, argv[0]);

    size_t len;
    const char *utf8 = JS_ToCStringLen(ctx, &len, input);
    JS_FreeValue(ctx, input);

    if (!utf8)
        return JS_EXCEPTION;

    if (len == 0) {
        JS_FreeCString(ctx, utf8);
        return JS_NewUint8Array(ctx, nullptr, 0, nullptr, nullptr, false);
    }

    auto *buffer = static_cast<uint8_t *>(js_malloc(ctx, len));
    if (!buffer) {
        JS_FreeCString(ctx, utf8);
        return JS_EXCEPTION;
    }

    memcpy(buffer, utf8, len);
    JS_FreeCString(ctx, utf8);

    return JS_NewUint8Array(
        ctx, buffer, len,
        [](JSRuntime *rt, void *, void *ptr) { js_free_rt(rt, ptr); },
        nullptr, false
    );
}

// ---------------------------------------------------------------------------
// TextEncoder.prototype.encodeInto
// ---------------------------------------------------------------------------
static JSValue js_encoder_encode_into(JSContext *ctx,
                                      JSValueConst this_val,
                                      int argc,
                                      JSValueConst *argv)
{
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "encodeInto requires source and destination");

    size_t utf8_len;
    const char *utf8 = JS_ToCStringLen(ctx, &utf8_len, argv[0]);
    if (!utf8)
        return JS_EXCEPTION;

    size_t dst_len;
    uint8_t *dst = JS_GetUint8Array(ctx, &dst_len, argv[1]);
    if (!dst) {
        JS_FreeCString(ctx, utf8);
        return JS_ThrowTypeError(ctx, "destination must be a Uint8Array");
    }

    size_t written = (utf8_len <= dst_len)
                     ? utf8_len
                     : utf8_safe_boundary(utf8, dst_len);

    memcpy(dst, utf8, written);

    uint32_t read_units = utf8_bytes_to_utf16_len(utf8, written);
    JS_FreeCString(ctx, utf8);

    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "read",    JS_NewUint32(ctx, read_units));
    JS_SetPropertyStr(ctx, result, "written", JS_NewUint32(ctx, static_cast<uint32_t>(written)));
    return result;
}

// ---------------------------------------------------------------------------
// TextEncoder.prototype.encoding (getter)
// ---------------------------------------------------------------------------
static JSValue js_encoder_get_encoding(JSContext *ctx,
                                       JSValueConst this_val)
{
    return JS_NewString(ctx, "utf-8");
}

// ---------------------------------------------------------------------------
// Class definition
// ---------------------------------------------------------------------------
static JSClassDef js_text_encoder_class = {
    .class_name = "TextEncoder",
    .finalizer = nullptr, // no native data to free
};

static const JSCFunctionListEntry js_text_encoder_proto_funcs[] = {
    JS_CGETSET_DEF("encoding", js_encoder_get_encoding, nullptr),
    JS_CFUNC_DEF("encode",     1, js_encoder_encode),
    JS_CFUNC_DEF("encodeInto", 2, js_encoder_encode_into),
};

// ---------------------------------------------------------------------------
// Public setup
// ---------------------------------------------------------------------------
void TextEncoder::SetupTextEncoder(JSContext *ctx, JSValue global)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    // Register the class once per runtime
    if (js_text_encoder_class_id == 0)
        JS_NewClassID(rt, &js_text_encoder_class_id);

    JS_NewClass(rt, js_text_encoder_class_id, &js_text_encoder_class);

    // Build the prototype
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto,
                               js_text_encoder_proto_funcs,
                               std::size(js_text_encoder_proto_funcs));

    // Build the constructor and wire up .prototype / @@toStringTag
    JSValue ctor = JS_NewCFunction2(ctx, js_text_encoder_ctor,
                                    "TextEncoder", 0,
                                    JS_CFUNC_constructor, 0);

    JS_SetConstructor(ctx, ctor, proto);           // ctor.prototype = proto, proto.constructor = ctor
    JS_SetClassProto(ctx, js_text_encoder_class_id, proto);  // JS_NewObjectProtoClass uses this

    JS_SetPropertyStr(ctx, global, "TextEncoder", ctor);
    // ctor is now owned by global; proto is owned by the class — don't free either
}