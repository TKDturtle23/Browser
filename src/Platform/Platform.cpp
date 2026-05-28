#include "Platform.h"



#if defined(__linux__)

#include "Platform_XCB.h"

#elif defined(_WIN32)


#endif

std::unique_ptr<Platform>
CreatePlatform() {

#if defined(__linux__)

    return std::make_unique<
        Platform_XCB
    >();

#elif defined(_WIN32)

    return std::make_unique<
        Platform_Win32
    >();

#else

    return nullptr;

#endif
}