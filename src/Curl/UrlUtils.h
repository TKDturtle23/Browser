#pragma once
#include <string>

namespace Engine::Utils {
    std::string ConvertUrlToCachePath(const std::string& url);
    std::string ResolveUrl(const std::string& baseUrl, const std::string& relUrl);
}