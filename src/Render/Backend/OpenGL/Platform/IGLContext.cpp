#include "IGLContext.h"
#include "Platform/Platform.h"

// Include concrete implementations for every supported platform.
// Guards ensure only the appropriate one compiles on each OS.
#ifdef _WIN32
  #include "Platform/Platform_Win32.h"
  #include "Win32GLContext.h"
#endif
// #ifdef __APPLE__
//   #include "Platform/Platform_Cocoa.h"
//   #include "CocoaGLContext.h"
// #endif
// #ifdef __linux__
//   #include "Platform/Platform_X11.h"
//   #include "X11GLContext.h"
// #endif

#include <stdexcept>

/*static*/
std::unique_ptr<IGLContext> IGLContext::CreateForPlatform(
        Platform* platform, void* sharedContextHandle) {

#ifdef _WIN32
    if (auto* win32 = dynamic_cast<Platform_Win32*>(platform)) {
        // sharedContextHandle is forwarded through CreateAndMakeCurrent(), not the constructor
        return std::make_unique<Win32GLContext>(win32);
    }
#endif

    throw std::runtime_error(
        "IGLContext::CreateForPlatform: no IGLContext implementation "
        "for the supplied Platform type on this build target.");
}