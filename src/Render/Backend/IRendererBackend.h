#pragma once

#include <cstdint>
#include <memory>

#include "Color.h"
#include "RenderTarget.h"
#include "Text/Font.h"

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
class Platform;

// ---------------------------------------------------------------------------
// ID types
// ---------------------------------------------------------------------------
using TextureID = uint32_t;
using WindowID  = uint32_t;

// ---------------------------------------------------------------------------
// Command types
// ---------------------------------------------------------------------------
enum class RenderCommandType {
    Clear,
    FillRect,
    DrawRect,
    FillRectBeveled,
    FillRectRounded,
    DrawLine,
    DrawPixel,
    DrawCircle,
    DrawGlyph,
    DrawTexture,
    BlitRenderTarget,
    DrawWavyLineInt,
    DrawWavyLineFloat,
};

struct ClearCommand             { Color color; };
struct FillRectCommand          { int x, y, w, h; Color color; };
struct DrawRectCommand          { int x, y, w, h; Color color; };
struct DrawLineCommand          { int x0, y0, x1, y1, thickness; Color color; };
struct DrawPixelCommand         { int x, y; Color color; };
struct DrawCircleCommand        { int cx, cy, radius; Color color; };
struct DrawGlyphCommand         { int x, y; Glyph glyph; Color color; };

struct FillRectBeveledCommand   { int x, y, w, h, radius; Color color; };
struct FillRectRoundedCommand   { int x, y, w, h, tl, tr, bl, br; Color color; };
struct BlitRenderTargetCommand {
    RenderTargetID source { InvalidRenderTarget };
    int dstX = 0, dstY = 0;
    int srcX = 0, srcY = 0;
    int w    = 0, h    = 0;
};

struct DrawWavyLineIntCommand {
    int   startX, y, endX;
    int   amplitude, wavelength;
    Color color;
};

struct DrawWavyLineFloatCommand {
    int   startX, endX;
    float startY, endY;
    float amplitude, frequency;
    int   thickness;
    Color color;
};

// Flat union-style aggregate — all sub-commands inline for cache locality.
// sizeof(RenderCommand) is intentionally large; sort by target, not by pointer.
struct RenderCommand {
    RenderTargetID    target { InvalidRenderTarget };
    RenderCommandType type   {};

    ClearCommand            clear           {};
    FillRectCommand         fillRect        {};
    DrawRectCommand         drawRect        {};
    FillRectBeveledCommand  fillRectBeveled {};
    DrawLineCommand         drawLine        {};
    DrawPixelCommand        drawPixel       {};
    DrawCircleCommand       drawCircle      {};
    DrawGlyphCommand        drawGlyph       {};
    BlitRenderTargetCommand blitRenderTarget{};
    DrawWavyLineIntCommand  drawWavyLineInt {};
    DrawWavyLineFloatCommand drawWavyLineFloat{};
    FillRectRoundedCommand fillRectRounded {};
};

// ---------------------------------------------------------------------------
// Backend selection
// ---------------------------------------------------------------------------
enum class PreferredBackend {
    Software,
    OpenGL,
    Vulkan,
};

// ---------------------------------------------------------------------------
// IRenderBackend
//
// Pure interface. Each implementation (OpenGL, Software, Vulkan, …) derives
// from this and is constructed via GetRenderBackend().
//
// Platform abstraction:
//   RegisterWindow accepts a Platform* — the backend is responsible for
//   extracting whatever OS handle it needs (HWND, NSView, xcb_window_t, …)
//   internally. No platform types leak into this header.
// ---------------------------------------------------------------------------
class IRenderBackend {
public:
    // Factory — implementation lives in IRenderBackend.cpp so this header
    // stays free of backend-specific includes.
    static std::shared_ptr<IRenderBackend> GetRenderBackend(
        PreferredBackend preferred = PreferredBackend::OpenGL);

    virtual ~IRenderBackend() = default;

    // ----- Window / surface management -----
    virtual WindowID RegisterWindow  (Platform* platform)                    = 0;
    virtual void     UnregisterWindow(WindowID window)                       = 0;
    virtual void     AttachRenderTarget(WindowID window, RenderTargetID target) = 0;

    // ----- Render target management -----
    virtual RenderTargetID CreateRenderTarget (int width, int height,
        bool blend = true)        = 0;
    virtual void           DestroyRenderTarget(RenderTargetID target)        = 0;
    virtual void           ResizeRenderTarget (RenderTargetID target,
                                               int width, int height)        = 0;
    virtual Color ReadPixel(RenderTargetID target, int x, int y, bool front) = 0;

    // ----- Frame lifecycle -----
    virtual void BeginFrame   ()                          = 0;
    virtual void SubmitCommand(const RenderCommand& cmd)  = 0;
    virtual void EndFrame     ()                          = 0;
    virtual void Present      ()                          = 0;

    // ----- Texture / atlas management -----
    // Creates a GPU-side texture sheet of the given dimensions.
    virtual TextureID CreateFontAtlas(int width, int height) = 0;

    // Uploads pixel data into a sub-region of an existing texture sheet.
    virtual void UpdateTextureSubImage(TextureID texture,
                                       int x, int y,
                                       int width, int height,
                                       const uint8_t* pixels) = 0;
};