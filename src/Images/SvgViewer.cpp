//
// Created by tkdtu on 5/29/2026.
//

#include "SvgViewer.h"
// Include nanoSVG implementation headers

#include <cstdint>
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgraster.h"

bool SvgViewer::IsSvg(const std::vector<uint8_t> &data) {
    if (data.size() < 4) return false;

    // Convert header to string for a quick check
    std::string header(data.begin(), data.begin() + std::min(data.size(), size_t(100)));
    return (header.find("<svg") != std::string::npos || header.find("<?xml") != std::string::npos);
}

std::vector<uint8_t> SvgViewer::GetPixels(const std::vector<uint8_t> &data, int width, int height, int &channels) {
    channels = 4; // NanoSVG rasters to 8-bit RGBA (4 channels)
    std::vector<uint8_t> pixels(width * height * channels, 0);

    // 1. Parse the SVG data from memory
    // NanoSVG expects a null-terminated string, so we create a copy with a terminator
    std::string svgStr(data.begin(), data.end());
    if (!image) {
        image = nsvgParse(const_cast<char*>(svgStr.c_str()), "px", 96.0f);
    }


    if (!image) {
        return pixels; // Return empty/black buffer on failure
    }

    // 2. Initialize the rasterizer
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    if (!rast) {
        nsvgDelete(image);
        return pixels;
    }

    // 3. Calculate scale factors to fit the target width and height
    float scaleX = (float)width / image->width;
    float scaleY = (float)height / image->height;
    // Use the smaller scale to maintain aspect ratio, or scale independently if forced
    float scale = std::min(scaleX, scaleY);

    // 4. Rasterize the vector paths into the pixel buffer
    // nsvgRasterize parameters: rasterizer, image, tx, ty, scale, dst, w, h, stride
    nsvgRasterize(rast, image, 0, 0, scale, pixels.data(), width, height, width * channels);

    // 5. Cleanup memory
    nsvgDeleteRasterizer(rast);

    return pixels;
}

bool SvgViewer::GetIntrinsicDimensions(const std::vector<uint8_t> &data, int &outWidth, int &outHeight) {
    std::string svgStr(data.begin(), data.end());
    image = nsvgParse(const_cast<char*>(svgStr.c_str()), "px", 96.0f);

    if (!image) return false;

    // Ceil or cast the float dimensions to integers
    outWidth = static_cast<int>(image->width);
    outHeight = static_cast<int>(image->height);


    return (outWidth > 0 && outHeight > 0);
}

void SvgViewer::clean() {
    nsvgDelete(image);
}
