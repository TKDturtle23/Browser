cmake_minimum_required(VERSION 4.2)

project(Browser)


set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_CXX_STANDARD 20)

# Must be BEFORE add_subdirectory()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(BUILD_SHARED_LIBS OFF)
set(CURL_STATICLIB ON)
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

add_executable(Browser
        src/main.cpp
        src/CurlGrabber.h
        src/CurlGrabber.cpp
        src/Tokenizer.cpp
        src/Parser.h
        src/Parser.cpp
        src/Platform/Platform_XCB.cpp
        src/Platform/Platform.cpp
        src/Render/Renderer.cpp
        src/Text/Font.cpp
        src/Layout/LayoutRenderer.cpp
        src/CSS/CSSTokenizer.cpp
        src/CSS/CSSParser.cpp
        src/Platform/Platform_win32.cpp
        src/BrowserCacheManager.cpp
        src/Window/ViewportManager.cpp
        src/Window/WindowManager.cpp
        src/Window/UI/InterfaceManager.cpp
        src/Images/SvgViewer.cpp
)
target_link_libraries(Browser PRIVATE freetype CURL::libcurl Euclase_ImageViewer)
target_include_directories(Browser PRIVATE "submodules/ImageLoader/src" "src/")



#target_link_libraries(Browser PRIVATE xcb CURL::libcurl)
