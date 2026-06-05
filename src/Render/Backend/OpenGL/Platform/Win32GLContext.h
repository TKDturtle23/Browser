#pragma once
#ifdef _WIN32

#include "IGLContext.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class Platform_Win32;

class Win32GLContext final : public IGLContext {
public:
    // Caller passes the Platform; shared context handle is supplied later
    // via CreateAndMakeCurrent() so the constructor stays trivial.
    explicit Win32GLContext(Platform_Win32* platform);
    ~Win32GLContext() override;

    // IGLContext
    void  CreateAndMakeCurrent(void* sharedContextHandle = nullptr) override;
    bool  MakeCurrent()                                             override;
    void  SwapBuffers()                                             override;
    int   GetWidth()  const                                         override;
    int   GetHeight() const                                         override;
    void* GetNativeContextHandle() const                            override;

private:
    Platform_Win32* platform = nullptr;

    HWND  hwnd  = nullptr;
    HDC   hdc   = nullptr;
    HGLRC hRC   = nullptr;
    bool  ownsRC = false;   // true only for the primary context

    void SetupPixelFormat();
};

#endif // _WIN32