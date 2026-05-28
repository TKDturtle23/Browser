//
// Created by tkdtu on 5/26/2026.
//

#ifndef BROWSER_CURLGRABBER_H
#define BROWSER_CURLGRABBER_H
#include <string>


struct Grab {
    std::string data;
};
class CurlGrabber {
    public:
    CurlGrabber() = default;

    void Init();
    Grab GetData(std::string link);
private:

};


#endif //BROWSER_CURLGRABBER_H
