#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <mutex>

namespace npc {

struct WebFetchResult {
    bool success = false;
    int status_code = 0;
    std::string url;
    std::string body;
    std::string content_type;
    std::string error;
    int64_t elapsed_ms = 0;
    size_t body_size = 0;
};

struct HtmlElement {
    std::string tag;
    std::string text;
    std::unordered_map<std::string, std::string> attrs;
    std::string href;
    std::string src;
};

struct WebSearchResult {
    std::string title;
    std::string url;
    std::string snippet;
};

struct ScrapeConfig {
    std::string url;
    std::string selector;
    bool extract_links = false;
    bool extract_text = true;
    bool extract_images = false;
    int max_items = 50;
    int timeout_ms = 15000;
};

struct ScrapeResult {
    bool success = false;
    std::string error;
    std::vector<HtmlElement> elements;
    std::vector<std::string> links;
    std::vector<std::string> images;
    std::string full_text;
    int items_found = 0;
    int64_t elapsed_ms = 0;

    std::string summary() const;
};

using BrowserActionCallback = std::function<bool(const std::string& action, const std::string& params)>;

class WebAutomation {
public:
    static WebAutomation& instance();

    void configure(const std::string& user_agent, int default_timeout_ms = 15000);

    WebFetchResult fetchUrl(const std::string& url,
                             const std::string& method = "GET",
                             const std::string& body = "",
                             const std::string& content_type = "application/json");
    WebFetchResult fetchGet(const std::string& url);
    WebFetchResult fetchPost(const std::string& url, const std::string& body);

    ScrapeResult scrape(const ScrapeConfig& config);
    ScrapeResult scrapePage(const std::string& url, const std::string& selector = "");

    std::string extractText(const std::string& html) const;
    std::vector<HtmlElement> extractElements(const std::string& html, const std::string& tag) const;
    std::vector<std::string> extractLinks(const std::string& html) const;
    std::vector<std::string> extractImages(const std::string& html) const;
    std::string extractTitle(const std::string& html) const;

    void setBrowserActionCallback(BrowserActionCallback cb);

    struct WebStats {
        int64_t total_requests = 0;
        int64_t success_count = 0;
        int64_t error_count = 0;
        int64_t total_bytes = 0;
        int64_t total_elapsed_ms = 0;
    };
    WebStats stats() const { return m_stats; }

private:
    WebAutomation() = default;
    WebAutomation(const WebAutomation&) = delete;
    WebAutomation& operator=(const WebAutomation&) = delete;

    std::string stripTags(const std::string& html) const;
    std::string collapseWhitespace(const std::string& text) const;
    std::string normalizeUrl(const std::string& base, const std::string& href) const;

    std::string m_user_agent = "NPC-World-WebAgent/1.0";
    int m_default_timeout = 15000;
    BrowserActionCallback m_browser_cb;
    WebStats m_stats;
    mutable std::mutex m_mutex;
};

} // namespace npc
