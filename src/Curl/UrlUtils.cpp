#include "UrlUtils.h"
#include <filesystem>

namespace Engine::Utils {

    std::string ConvertUrlToCachePath(const std::string& url) {
        std::string safeName = url;
        for (char& c : safeName) {
            if (c == '/' || c == '\\' || c == ':' || c == '?' || c == '*' || c == '&') {
                c = '_';
            }
        }
        std::filesystem::create_directories("image_cache");
        return "image_cache/" + safeName;
    }

    std::string ResolveUrl(const std::string& baseUrl, const std::string& relUrl) {
        if (relUrl.rfind("https://", 0) == 0 || relUrl.rfind("http://", 0) == 0) {
            return relUrl;
        }
        if (relUrl.rfind("//", 0) == 0) {
            size_t protoEnd = baseUrl.find("://");

            std::string protocol = (protoEnd != std::string::npos) ? baseUrl.substr(0, protoEnd) : "https";
            return protocol + ":" + relUrl;
        }
        size_t protoEnd = baseUrl.find("://");
        std::string protocol = "https", rest = baseUrl;

        if (protoEnd != std::string::npos) {
            protocol = baseUrl.substr(0, protoEnd);
            rest = baseUrl.substr(protoEnd + 3);
        }
        size_t slashPos = rest.find('/');

        std::string host = (slashPos != std::string::npos) ? rest.substr(0, slashPos) : rest;
        std::string path = (slashPos != std::string::npos) ? rest.substr(slashPos) : "/";

        if (!relUrl.empty() && relUrl[0] == '/') {
            return protocol + "://" + host + relUrl;
        }
        size_t lastSlash = path.find_last_of('/');

        std::string dir = (lastSlash != std::string::npos) ? path.substr(0, lastSlash + 1) : "/";
        return protocol + "://" + host + dir + relUrl;
    }

}