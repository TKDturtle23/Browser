cmake_minimum_required(VERSION 3.20) # Rolled back to 3.20+ for broader compatibility, adjust if 3.24+ features are required

project(Browser LANGUAGES CXX)
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()
# -------------------------------------------------------------------
# 1. Global Project & Compiler Configurations
# -------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
# Force all submodules to build as static libraries
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries only" FORCE)
# -------------------------------------------------------------------
# 3. Third-Party Submodules (CURL, FreeType, WebP)
# -------------------------------------------------------------------
set(CURL_STATICLIB ON)

# Configure internal submodule behaviors safely using CACHE FORCE
set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)

add_subdirectory(submodules/curl)
add_subdirectory(submodules/freetype)
add_subdirectory(submodules/libwebp)
add_subdirectory(submodules/ImageLoader/src)
add_subdirectory(submodules/quickjs)
# -------------------------------------------------------------------
# 4. Main Executable Target & Sources
# -------------------------------------------------------------------
add_executable(Browser
        src/main.cpp
        src/BrowserCacheManager.cpp
        src/CurlGrabber.h
        src/CurlGrabber.cpp
        src/Tokenizer.cpp
        src/Parser.h
        src/Parser.cpp
        src/CSS/CSSTokenizer.cpp
        src/CSS/CSSParser.cpp
        src/Layout/LayoutRenderer.cpp
        src/Images/SvgViewer.cpp
        src/Text/Font.cpp
        src/Render/Renderer.cpp
        src/Window/ViewportManager.cpp
        src/Window/WindowManager.cpp
        src/Window/UI/InterfaceManager.cpp
        src/Platform/Platform.cpp
        src/Platform/Platform_win32.cpp
        src/Platform/Platform_XCB.cpp
        src/JavaScriptEngine/JavaScriptEngine.cpp
        src/JavaScriptEngine/QuickjsEngine.cpp
        src/JavaScriptEngine/JS_Functions.cpp
        src/NodeToHTML.cpp
        src/Window/Debugwindowmanager.cpp
        src/Logger.cpp
)


# Target Include Directories
target_include_directories(Browser PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/submodules/ImageLoader/src"
        submodules/quickjs
)

# Target Link Libraries
target_link_libraries(Browser PRIVATE
        freetype
        CURL::libcurl
        Euclase_ImageViewer
        qjs                  # <--- The engine core target
        qjs-libc             # <--- Standard JS modules (console, string tools, etc.)
)

# System library additions for Windows
if(WIN32)
    target_link_libraries(Browser PRIVATE dbghelp.lib shlwapi.lib winmm.lib user32.lib gdi32.lib)
endif()

