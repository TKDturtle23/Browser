cmake_minimum_required(VERSION 4.2)

project(Browser)

set(CMAKE_CXX_STANDARD 20)

# Must be BEFORE add_subdirectory()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(BUILD_SHARED_LIBS OFF)
set(CURL_STATICLIB ON)

set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)


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
)
find_package(Freetype REQUIRED)
find_package(CURL REQUIRED)
target_link_libraries(Browser PRIVATE Freetype::Freetype)



#target_link_libraries(Browser PRIVATE xcb CURL::libcurl)
