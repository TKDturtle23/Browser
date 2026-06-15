//
// Created by tkdtu on 5/26/2026.
//

#ifndef BROWSER_CURLGRABBER_H
#define BROWSER_CURLGRABBER_H
#include <map>
#include <optional>
#include <string>
#include <vector>

struct DebugNetEntry {
    std::string method;       // "GET", "POST", ...
    std::string url;
    int         statusCode  = 0;   // 0 = pending/failed
    std::string contentType;       // "text/html", "script", ...
    int         sizeBytes   = 0;
    int         timeMs      = 0;
};
struct Grab {
    std::string body;
    std::map<std::string, std::string> headers;
    long status_code;
    DebugNetEntry netDebug;
};
class CurlGrabber {
    public:
    CurlGrabber() = default;

    static void Init();
    static Grab GetData(std::string link, const std::map<std::string, std::string> &extraHeaders);
    static std::vector<Grab> GetGrabLog();
    static void ResetLog();
private:
    static std::vector<Grab> Grab_Log;
};


#endif //BROWSER_CURLGRABBER_H
