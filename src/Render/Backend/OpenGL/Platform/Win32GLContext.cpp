#ifdef _WIN32
#include "Win32GLContext.h"
#include "Platform/Platform_Win32.h"
#include "glad/wgl.h"
#include "glad/gl.h"
#include <stdexcept>

Win32GLContext::Win32GLContext(Platform_Win32* platform)
    : platform(platform) {}

Win32GLContext::~Win32GLContext() {
    // Only the primary context owns hRC — secondaries borrow it.
    if (hRC && ownsRC) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(hRC);
    }
    if (hdc && hwnd)
        ReleaseDC(hwnd, hdc);
}

void Win32GLContext::SetupPixelFormat() {
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |
                       PFD_DOUBLEBUFFER   | PFD_SUPPORT_COMPOSITION;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;
    pfd.cDepthBits   = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType   = PFD_MAIN_PLANE;

    const int fmt = ChoosePixelFormat(hdc, &pfd);
    if (!fmt || !SetPixelFormat(hdc, fmt, &pfd))
        throw std::runtime_error("Win32GLContext: failed to set pixel format.");
}

void Win32GLContext::CreateAndMakeCurrent(void* sharedContextHandle) {
    hwnd = reinterpret_cast<HWND>(platform->GetNativeHandle());
    hdc  = ::GetDC(hwnd);
    if (!hdc) throw std::runtime_error("Win32GLContext: GetDC failed.");

    SetupPixelFormat();

    HGLRC sharedRC = static_cast<HGLRC>(sharedContextHandle);

    if (!sharedRC) {
        // ── Primary context ──
        // Bootstrap WGL extensions via a throwaway legacy context, then
        // create a proper core-profile context and load all GL entry points.

        HGLRC tempRC = wglCreateContext(hdc);
        if (!tempRC) throw std::runtime_error("Win32GLContext: temp context creation failed.");
        wglMakeCurrent(hdc, tempRC);

        if (!gladLoadWGL(hdc, reinterpret_cast<GLADloadfunc>(wglGetProcAddress))) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tempRC);
            throw std::runtime_error("Win32GLContext: gladLoadWGL failed.");
        }

        const int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 6,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            WGL_CONTEXT_FLAGS_ARB,
#ifdef _DEBUG
            WGL_CONTEXT_DEBUG_BIT_ARB,
#else
            0,
#endif
            0
        };

        hRC = wglCreateContextAttribsARB(hdc, nullptr, attribs);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tempRC);

        if (!hRC) throw std::runtime_error("Win32GLContext: core-profile context creation failed.");

        wglMakeCurrent(hdc, hRC);

        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(wglGetProcAddress)))
            throw std::runtime_error("Win32GLContext: gladLoadGL failed.");

        ownsRC = true;

    } else {
        // ── Secondary surface ──
        // WGL requires each surface to have the same pixel format as the primary,
        // which SetupPixelFormat() above already handles. We reuse the primary's
        // HGLRC directly — wglShareLists is not needed because we're using the
        // same context object, not a separate one with shared namespaces.
        hRC    = sharedRC;
        ownsRC = false;

        if (!wglMakeCurrent(hdc, hRC))
            throw std::runtime_error("Win32GLContext: wglMakeCurrent failed for secondary surface.");
    }
}

bool Win32GLContext::MakeCurrent() {
    // Skip the kernel call if we're already current — wglMakeCurrent is not free.
    if (wglGetCurrentDC() == hdc && wglGetCurrentContext() == hRC)
        return true;
    return wglMakeCurrent(hdc, hRC) == TRUE;
}

void Win32GLContext::SwapBuffers() {
    ::SwapBuffers(hdc);
}

int Win32GLContext::GetWidth()  const { return platform->GetWidth();  }
int Win32GLContext::GetHeight() const { return platform->GetHeight(); }

void* Win32GLContext::GetNativeContextHandle() const { return hRC; }

#endif