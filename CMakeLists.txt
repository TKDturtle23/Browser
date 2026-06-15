cmake_minimum_required(VERSION 3.20) # Rolled back to 3.20+ for broader compatibility, adjust if 3.24+ features are required

project(Browser LANGUAGES C CXX)

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
        src/Curl/BrowserCacheManager.cpp
        src/Curl/CurlGrabber.h
        src/Curl/CurlGrabber.cpp
        src/Tokenizer.cpp
        src/Parser.h
        src/Parser.cpp
        src/CSS/CSSTokenizer.cpp
        src/CSS/CSSParser.cpp
        src/Layout/LayoutGenerator.cpp
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
        src/Window/Debug/Debugwindowmanager.cpp
        src/Debug/Logger.cpp
        src/Debug/Timer.cpp
        src/Layout/WordCollector.cpp
        src/Layout/LayoutHelper.cpp
        src/Layout/Context/InlineFormattingContext.cpp
        src/Layout/Context/BlockFormattingContext.cpp
        src/Layout/LayoutRenderer.cpp
        src/Layout/Context/FontManager.cpp
        src/Layout/Context/FontManager.h
        src/Layout/Context/FontManager.cpp
        src/Render/Backend/IRendererBackend.h
        src/Render/Backend/OpenGL/OpenGLBackend.cpp
        src/Render/Backend/OpenGL/OpenGLBackend.h
        src/Render/Backend/Software/SoftwareBackend.cpp
        src/Render/Backend/Software/SoftwareBackend.h
        src/Render/Backend/IRenderBackend.cpp
        src/Render/Backend/RenderTarget.h
        src/Platform/GLAD/src/wgl.c
        src/Platform/GLAD/src/gl.c
        src/Render/Backend/OpenGL/Platform/IGLContext.h
        src/Render/Backend/OpenGL/Platform/Win32GLContext.cpp
        src/Render/Backend/OpenGL/Platform/Win32GLContext.h
        src/Render/Backend/OpenGL/Platform/IGLContext.cpp
        src/Curl/UrlUtils.cpp
        src/Text/TextSelector.cpp
        src/Window/Debug/DebugOverlayRenderer.cpp
        src/Window/Debug/DebugOverlayRenderer.h
        src/Platform/EventDispatcher.cpp
        src/Platform/EventDispatcher.h
        src/Layout/Context/FlexFormattingContext.cpp
        src/Layout/Context/FlexFormattingContext.h
        src/Node/Flex.h
        src/Node/CSSLength.h
        src/CSS/CSSColor.h
        src/CSS/CSSColor.cpp
)

# Target Include Directories
target_include_directories(Browser PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/submodules/ImageLoader/src"
        submodules/quickjs
        "src/Platform/GLAD/include"
)

# Target Link Libraries
target_link_libraries(Browser PRIVATE
        freetype
        CURL::libcurl
        Euclase_ImageViewer
        qjs                  # <--- The engine core target
        qjs-libc             # <--- Standard JS modules (console, string tools, etc.)
        dwmapi
        opengl32
)

# System library additions for Windows
if(WIN32)
    target_link_libraries(Browser PRIVATE dbghelp.lib shlwapi.lib winmm.lib user32.lib gdi32.lib)
endif()

