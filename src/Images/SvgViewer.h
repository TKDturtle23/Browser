#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <stdint.h>



class SvgViewer {
public:
    // Simple signature check: SVG files usually start with '<svg' or XML declarations
    static bool IsSvg(const std::vector<uint8_t> &data);

     std::vector<uint8_t> GetPixels(const std::vector<uint8_t>& data, int width, int height, int& channels);
     bool GetIntrinsicDimensions(const std::vector<uint8_t>& data, int& outWidth, int& outHeight);

    void clean();
private:
    struct NSVGimage* image{};
};
