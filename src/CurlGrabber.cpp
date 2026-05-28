//
// Created by tkdtu on 5/26/2026.
//

#include "CurlGrabber.h"

#include <curl/curl.h>
#include <sstream>
#include <string>

// Callback for writing received data into a std::string
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

void CurlGrabber::Init() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

Grab CurlGrabber::GetData(std::string link) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) {
        return {""};
    }

    curl_easy_setopt(curl, CURLOPT_URL, link.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Optional but commonly needed
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        return {""};
    }

    curl_easy_cleanup(curl);
    return {response};
}