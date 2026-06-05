#pragma once

#include <memory>

class Platform;

// ---------------------------------------------------------------------------
// IGLContext
//
// Platform-agnostic interface for an OpenGL window surface + context pair.
// One concrete implementation per OS:
//   Win32GLContext   — WGL  (Windows)
//   CocoaGLContext   — NSOpenGL / CGL (macOS)
//   X11GLContext     — GLX  (Linux)
//   WaylandGLContext — EGL  (Linux/Wayland)
//
// Lifetime contract:
//   CreateForPlatform() returns a fully constructed but uninitialised context.
//   CreateAndMakeCurrent() must be called before any GL work.
//   The primary window calls CreateAndMakeCurrent() with sharedContext=nullptr.
//   Additional windows pass the primary context handle so objects are shared.
// ---------------------------------------------------------------------------
class IGLContext {
public:
    virtual ~IGLContext() = default;

    // Factory — inspects the concrete Platform subtype at runtime and returns
    // the matching IGLContext implementation. Throws if the platform is unknown
    // or unsupported on the current build target.
    static std::unique_ptr<IGLContext> CreateForPlatform(
        Platform*  platform,
        void*      sharedContextHandle = nullptr);

    // Bootstrap: create a core-profile GL context and make it current.
    // sharedContextHandle is the opaque handle from an existing primary context
    // (used for object sharing across surfaces). Pass nullptr for the first window.
    // Throws std::runtime_error on failure.
    virtual void CreateAndMakeCurrent(void* sharedContextHandle = nullptr) = 0;

    // Redirect subsequent GL calls to this surface.
    // Returns false if the switch failed (log and skip, don't crash).
    virtual bool MakeCurrent() = 0;

    // Swap front/back buffers.
    virtual void SwapBuffers() = 0;

    // Current client-area size (may change on resize — query each frame if needed).
    virtual int GetWidth()  const = 0;
    virtual int GetHeight() const = 0;

    // Opaque native context handle (HGLRC, NSOpenGLContext*, GLXContext, …).
    // Used only to pass to CreateAndMakeCurrent of secondary contexts.
    // Returns nullptr until CreateAndMakeCurrent() succeeds.
    virtual void* GetNativeContextHandle() const = 0;
};