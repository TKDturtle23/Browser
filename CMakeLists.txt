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

add_subdirectory(submodules/curl)
add_subdirectory(submodules/curlpp)

add_executable(Browser
        src/main.cpp
)

target_link_libraries(Browser PRIVATE
        curlpp
        libcurl
)