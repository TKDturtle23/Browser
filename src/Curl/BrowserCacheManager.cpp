#include "BrowserCacheManager.h"

#include <fstream>
#include <sstream>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <filesystem>

#ifdef ENABLE_VERBOSE_LOGGING
    #define LOG_VERBOSE(msg) std::cout << msg << std::endl
#else
    #define LOG_VERBOSE(msg) do {} while (0)
#endif

BrowserCacheManager::BrowserCacheManager(const std::string& cacheDir) : cacheDirectory(cacheDir) {
    if (!cacheDirectory.empty() && cacheDirectory.back() != '/' && cacheDirectory.back() != '\\') {
        cacheDirectory += "/";
    }
    indexFilePath = cacheDirectory + "cache_index.txt";
    LoadIndex();
}

BrowserCacheManager::~BrowserCacheManager() {
    SaveIndex();
}

// Upgraded to explicit 64-bit to prevent address collision issues across massive sites
std::string BrowserCacheManager::HashUrl(const std::string& url) {
    uint64_t hash = 5381;
    for (char c : url) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }
    return std::to_string(hash);
}

long long BrowserCacheManager::GetCurrentUnixTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
std::string BrowserCacheManager::LoadOfflinePage() {
    std::filesystem::path offlinePath =
        std::filesystem::current_path() / "offline.html";

    std::string offlineContent =
        ReadFileFromDisk(offlinePath.string());

    if (!offlineContent.empty()) {
        return offlineContent;
    }

    return R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Unable to Load</title>
    <style>
        body {
            margin: 0;
            background: #111;
            color: #eee;
            font-family: sans-serif;
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            height: 100vh;
            text-align: center;
        }

        h1 {
            font-size: 48px;
            margin-bottom: 10px;
        }

        p {
            color: #aaa;
            font-size: 18px;
        }
    </style>
</head>
<body>
    <h1>Unable to connect</h1>
    <p>The browser could not load the requested page.</p>
</body>
</html>
)";
}
long long BrowserCacheManager::ParseMaxAge(const std::string& cacheControlHeader) {
    std::string headerCopy = cacheControlHeader;
    std::transform(headerCopy.begin(), headerCopy.end(), headerCopy.begin(), ::tolower);

    size_t pos = headerCopy.find("max-age=");
    if (pos != std::string::npos) {
        std::string maxAgeStr = headerCopy.substr(pos + 8);
        size_t endPos = maxAgeStr.find_first_not_of("0123456789");

        try {
            long long parsedAge = std::stoll(maxAgeStr.substr(0, endPos));
            return (parsedAge == 0) ? 5 : parsedAge;
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

// Helper utility to safely extract and map values regardless of server casing mismatches
static std::string GetHeaderValueValueInsensitive(const std::map<std::string, std::string>& headers, const std::string& key) {
    for (const auto& [k, v] : headers) {
        std::string lowerK = k;
        std::transform(lowerK.begin(), lowerK.end(), lowerK.begin(), ::tolower);
        if (lowerK == key) return v;
    }
    return "";
}

std::string BrowserCacheManager::GetResource(const std::string& url) {
    std::string urlHash = HashUrl(url);
    long long currentTime = GetCurrentUnixTime();

    if (cacheIndex.find(urlHash) != cacheIndex.end()) {
        CacheMetadata& meta = cacheIndex[urlHash];
        std::string localPath = cacheDirectory + meta.filename;

        if (currentTime < meta.expires_at) {
            LOG_VERBOSE("[CACHE HIT] Fresh asset loaded from disk: " << url);
            return ReadFileFromDisk(localPath);
        }

        LOG_VERBOSE("[CACHE EXPIRED] Revalidating asset with server: " << url);
        std::map<std::string, std::string> conditionalHeaders;
        if (!meta.etag.empty()) {
            conditionalHeaders["If-None-Match"] = meta.etag;
        }
        if (!meta.last_modified.empty()) {
            conditionalHeaders["If-Modified-Since"] = meta.last_modified;
        }

        CurlGrabber grabber;
        Grab response = grabber.GetData(url, conditionalHeaders);
        // NETWORK FAILURE
        if (response.status_code <= 0) {
            LOG_VERBOSE("[NETWORK ERROR] Loading offline page.");

            // Prefer stale cache over offline page
            std::string staleCache = ReadFileFromDisk(localPath);

            if (!staleCache.empty()) {
                return staleCache;
            }

            return LoadOfflinePage();
        }
        std::string cacheControl = GetHeaderValueValueInsensitive(response.headers, "cache-control");
        std::string etagVal = GetHeaderValueValueInsensitive(response.headers, "etag");
        std::string lastModVal = GetHeaderValueValueInsensitive(response.headers, "last-modified");

        if (response.status_code == 304) {
            LOG_VERBOSE("[CACHE HIT] Server returned 304 Not Modified. Renewing cache.");
            long long maxAge = 3600;
            if (!cacheControl.empty()) {
                maxAge = ParseMaxAge(cacheControl);
            }
            meta.expires_at = currentTime + maxAge;
            SaveIndex();
            return ReadFileFromDisk(localPath);
        }
        else if (response.status_code == 200) {
            LOG_VERBOSE("[CACHE MISS] Content updated. Updating local file.");
            WriteFileToDisk(localPath, response.body);

            meta.expires_at = currentTime + (!cacheControl.empty() ? ParseMaxAge(cacheControl) : 3600);
            meta.etag = etagVal;
            meta.last_modified = lastModVal;
            SaveIndex();
            return response.body;
        }
    }

    LOG_VERBOSE("[CACHE MISS] Fetching fresh network resource: " << url);
    CurlGrabber grabber;
    Grab response = grabber.GetData(url, {});
    // NETWORK FAILURE
    if (response.status_code <= 0) {
        LOG_VERBOSE("[NETWORK ERROR] Loading offline page.");
        return LoadOfflinePage();
    }
    std::string cacheControl = GetHeaderValueValueInsensitive(response.headers, "cache-control");
    std::string etagVal = GetHeaderValueValueInsensitive(response.headers, "etag");
    std::string lastModVal = GetHeaderValueValueInsensitive(response.headers, "last-modified");

    if (response.status_code == 200) {
        if (!cacheControl.empty() && cacheControl.find("no-store") != std::string::npos) {
            return response.body;
        }

        std::string filename = urlHash + ".cache";
        std::string localPath = cacheDirectory + filename;
        WriteFileToDisk(localPath, response.body);

        CacheMetadata meta;
        meta.url = url;
        meta.filename = filename;

        long long maxAge = 3600;
        if (!cacheControl.empty()) {
            maxAge = ParseMaxAge(cacheControl);
        }
        meta.expires_at = currentTime + maxAge;
        meta.etag = etagVal;
        meta.last_modified = lastModVal;

        cacheIndex[urlHash] = meta;
        SaveIndex();
    }
    // Non-200 responses fallback
    if (response.status_code != 200) {
        return LoadOfflinePage();
    }

    return response.body;
}

void BrowserCacheManager::WriteFileToDisk(const std::string& filepath, const std::string& content) {
    std::ofstream out(filepath, std::ios::out | std::ios::binary);
    if (out) {
        out.write(content.data(), content.size());
    }
}

std::string BrowserCacheManager::ReadFileFromDisk(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::in | std::ios::binary | std::ios::ate);
    if (!in) return "";

    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::string content;
    content.resize(size);

    if (in.read(&content[0], size)) {
        return content;
    }
    return "";
}

void BrowserCacheManager::LoadIndex() {
    std::ifstream in(indexFilePath);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string hash, url, file, etag, last_mod;
        std::string expStr;

        // Using explicit tabs '\t' for index structure serialization safety
        if (std::getline(ss, hash, '\t') &&
            std::getline(ss, url, '\t') &&
            std::getline(ss, file, '\t') &&
            std::getline(ss, expStr, '\t') &&
            std::getline(ss, etag, '\t') &&
            std::getline(ss, last_mod)) {
            try {
                long long exp = std::stoll(expStr);
                cacheIndex[hash] = {url, file, exp, etag, last_mod};
            } catch (...) {
                continue;
            }
        }
    }
}

void BrowserCacheManager::SaveIndex() {
    std::ofstream out(indexFilePath);
    if (!out) return;
    for (const auto& [hash, meta] : cacheIndex) {
        out << hash << "\t" << meta.url << "\t" << meta.filename << "\t" << meta.expires_at << "\t"
            << meta.etag << "\t" << meta.last_modified << "\n";
    }
}