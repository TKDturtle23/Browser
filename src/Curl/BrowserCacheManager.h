#ifndef BROWSER_CACHE_MANAGER_H
#define BROWSER_CACHE_MANAGER_H

#include <string>
#include <map>

#include "CurlGrabber.h"

class BrowserCacheManager {
public:
    // Pass the directory where you want to save cached files (e.g., "./cache/")
    BrowserCacheManager(const std::string& cacheDir);
    ~BrowserCacheManager();

    // The clean primary interface for your browser engine
    std::string GetResource(const std::string& url);

private:
    std::string cacheDirectory;
    std::string indexFilePath;
    
    // An in-memory cache index mapped by URL hash
    // In production, parse/save this to index.json using a JSON library like nlohmann/json
    struct CacheMetadata {
        std::string url;
        std::string filename;
        long long expires_at; // Unix timestamp
        std::string etag;
        std::string last_modified;
    };
    std::map<std::string, CacheMetadata> cacheIndex;

    // Helper functions
    static std::string HashUrl(const std::string& url);
    void LoadIndex();
    void SaveIndex();

    static long long GetCurrentUnixTime();

    std::string LoadOfflinePage();

    static long long ParseMaxAge(const std::string& cacheControlHeader);
    
    std::string ReadFileFromDisk(const std::string& filepath);
    void WriteFileToDisk(const std::string& filepath, const std::string& content);
};

#endif // BROWSER_CACHE_MANAGER_H