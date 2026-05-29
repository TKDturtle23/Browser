//
// Created by tkdtu on 5/26/2026.
//

#ifndef BROWSER_CURLGRABBER_H
#define BROWSER_CURLGRABBER_H
#include <map>
#include <string>


struct Grab {
    std::string body;
    std::map<std::string, std::string> headers;
    long status_code;
};
class CurlGrabber {
    public:
    CurlGrabber() = default;

    static void Init();
    Grab GetData(std::string link, const std::map<std::string, std::string> &extraHeaders);
private:

};


#endif //BROWSER_CURLGRABBER_H
