#include "Curl/CurlGrabber.h"
#include <iostream>
#include <curl/curl.h>
#include <sstream>
#include <string>
#include <map>
#include <algorithm>
#include <chrono>
// Callback for the body
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// Callback for headers (called once per header line)
static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t totalSize = size * nitems;
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);

    std::string line(buffer, totalSize);

    // Find the delimiter between key and value
    size_t colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        // Trim whitespace and newlines (libcurl headers end in \r\n)
        auto trim = [](std::string& s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
        };

        trim(key);
        trim(value);

        // Turn key lowercase for easy case-insensitive lookups later
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        (*headers)[key] = value;
    }

    return totalSize;
}

void CurlGrabber::Init() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::cout << curl_version() << std::endl;
}
Grab CurlGrabber::GetData(std::string link, const std::map<std::string, std::string>& extraHeaders) {
    CURL* curl = curl_easy_init();
    std::string response_body;
    std::map<std::string, std::string> response_headers;
    long status_code = 0;

    if (!curl) return {"", {}, 0, {}};

    std::string user_agent = "EuclaseBrowser/1.0";
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, link.c_str());

    // Body & Header Callbacks setup
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);

    struct curl_slist* chunk = nullptr;
    for (const auto& [key, val] : extraHeaders) {
        std::string header_line = key + ": " + val;
        chunk = curl_slist_append(chunk, header_line.c_str());
    }
    if (chunk) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // ─── START METRIC RECORDING ───
    auto startTime = std::chrono::high_resolution_clock::now();

    CURLcode res = curl_easy_perform(curl);

    auto endTime = std::chrono::high_resolution_clock::now();
    int elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    // ─── END METRIC RECORDING ───

    if (res != CURLE_OK) {
        std::cerr << curl_easy_strerror(res) << std::endl;
        if (chunk) curl_slist_free_all(chunk);
        curl_easy_cleanup(curl);

        // Return an entry tracking the failure
        DebugNetEntry failedEntry{"GET", link, 0, "unknown", 0, elapsedMs};
        return {"", {}, 0, failedEntry};
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    // ─── POPULATE DEBUGNETENTRY STRUCT ───
    DebugNetEntry netEntry;
    netEntry.method = "GET"; // Update dynamically if you implement POST routes later
    netEntry.url = link;
    netEntry.statusCode = static_cast<int>(status_code);
    netEntry.timeMs = elapsedMs;

    // Total size of the body payload gathered
    netEntry.sizeBytes = static_cast<int>(response_body.size());

    // Extract Content-Type safely from the map parsed by HeaderCallback
    // Your callback forces keys to lowercase, making lookup reliable
    if (response_headers.contains("content-type")) {
        std::string rawContentType = response_headers["content-type"];

        // Strip optional parameters like encoding (e.g., "text/html; charset=UTF-8")
        size_t semiColonPos = rawContentType.find(';');
        if (semiColonPos != std::string::npos) {
            rawContentType = rawContentType.substr(0, semiColonPos);
        }
        netEntry.contentType = rawContentType;
    } else {
        netEntry.contentType = "unknown";
    }

    // Cleanup resources
    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);

    // Package everything together
    auto grab = Grab{response_body, response_headers, status_code, netEntry};
    Grab_Log.push_back(grab);
    return grab;
}

void CurlGrabber::ResetLog() {
    Grab_Log.clear();
}

std::vector<Grab> CurlGrabber::Grab_Log;
std::vector<Grab> CurlGrabber::GetGrabLog() {
    return Grab_Log;

}
