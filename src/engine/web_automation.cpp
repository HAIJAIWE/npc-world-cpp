#include "engine/web_automation.h"
#include "engine/api_client.h"
#include <sstream>
#include <regex>
#include <algorithm>
#include <chrono>

namespace npc {

WebAutomation& WebAutomation::instance() {
    static WebAutomation inst;
    return inst;
}

void WebAutomation::configure(const std::string& user_agent, int default_timeout_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_user_agent = user_agent;
    m_default_timeout = default_timeout_ms;
}

WebFetchResult WebAutomation::fetchUrl(const std::string& url,
                                        const std::string& method,
                                        const std::string& body,
                                        const std::string& content_type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_requests++;

    WebFetchResult result;
    result.url = url;

    auto start = std::chrono::steady_clock::now();

    auto& client = ApiClient::instance();
    auto raw = client.call(url, method, body, content_type, m_default_timeout);

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    result.success = raw.success;
    result.status_code = raw.status_code;
    result.body = raw.body;
    result.body_size = raw.body.size();

    if (raw.success) {
        m_stats.success_count++;
        m_stats.total_bytes += raw.body.size();
    } else {
        result.error = raw.error;
        m_stats.error_count++;
    }

    m_stats.total_elapsed_ms += result.elapsed_ms;
    return result;
}

WebFetchResult WebAutomation::fetchGet(const std::string& url) {
    return fetchUrl(url, "GET");
}

WebFetchResult WebAutomation::fetchPost(const std::string& url, const std::string& body) {
    return fetchUrl(url, "POST", body);
}

ScrapeResult WebAutomation::scrapePage(const std::string& url, const std::string& selector) {
    ScrapeConfig cfg;
    cfg.url = url;
    cfg.selector = selector;
    cfg.extract_links = true;
    cfg.extract_text = true;
    cfg.extract_images = true;
    cfg.timeout_ms = m_default_timeout;
    return scrape(cfg);
}

ScrapeResult WebAutomation::scrape(const ScrapeConfig& config) {
    ScrapeResult result;
    auto start = std::chrono::steady_clock::now();

    auto fetch = fetchGet(config.url);
    if (!fetch.success) {
        result.error = fetch.error;
        return result;
    }

    result.full_text = extractText(fetch.body);

    if (config.extract_links) {
        result.links = extractLinks(fetch.body);
    }

    if (config.extract_images) {
        result.images = extractImages(fetch.body);
    }

    if (!config.selector.empty()) {
        result.elements = extractElements(fetch.body, config.selector);
    }

    if (config.max_items > 0 && (int)result.links.size() > config.max_items) {
        result.links.resize(config.max_items);
    }
    if (config.max_items > 0 && (int)result.images.size() > config.max_items) {
        result.images.resize(config.max_items);
    }

    result.items_found = (int)(result.links.size() + result.images.size() + result.elements.size());
    result.success = true;

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    return result;
}

std::string WebAutomation::stripTags(const std::string& html) const {
    std::string result;
    result.reserve(html.size());
    bool in_tag = false;
    for (size_t i = 0; i < html.size(); ++i) {
        if (html[i] == '<') {
            in_tag = true;
        } else if (html[i] == '>') {
            in_tag = false;
        } else if (!in_tag) {
            if (html[i] == '&') {
                size_t end = html.find(';', i);
                if (end != std::string::npos) {
                    std::string entity = html.substr(i + 1, end - i - 1);
                    if (entity == "amp") result += '&';
                    else if (entity == "lt") result += '<';
                    else if (entity == "gt") result += '>';
                    else if (entity == "quot") result += '"';
                    else if (entity == "apos") result += '\'';
                    else if (entity == "nbsp") result += ' ';
                    i = end;
                    continue;
                }
            }
            result += html[i];
        }
    }
    return result;
}

std::string WebAutomation::collapseWhitespace(const std::string& text) const {
    std::string result;
    result.reserve(text.size());
    bool last_was_space = false;
    for (char c : text) {
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            if (!last_was_space) {
                result += ' ';
                last_was_space = true;
            }
        } else {
            result += c;
            last_was_space = false;
        }
    }
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

std::string WebAutomation::extractText(const std::string& html) const {
    return collapseWhitespace(stripTags(html));
}

std::string WebAutomation::extractTitle(const std::string& html) const {
    std::regex title_re(R"(<title[^>]*>([\s\S]*?)</title>)", std::regex::icase);
    std::smatch match;
    if (std::regex_search(html, match, title_re)) {
        return collapseWhitespace(stripTags(match[1].str()));
    }
    return "";
}

std::vector<HtmlElement> WebAutomation::extractElements(const std::string& html, const std::string& tag) const {
    std::vector<HtmlElement> result;
    std::regex elem_re("<" + tag + R"([^>]*?)>([\s\S]*?)</)" + tag + ">",
                       std::regex::icase);
    std::sregex_iterator it(html.begin(), html.end(), elem_re);
    std::sregex_iterator end;
    for (; it != end; ++it) {
        HtmlElement el;
        el.tag = tag;
        el.text = collapseWhitespace(stripTags((*it)[1].str()));

        std::string elem_str = (*it)[0].str();
        std::regex href_re(R"(href\s*=\s*["']([^"']+)["'])", std::regex::icase);
        std::smatch hm;
        if (std::regex_search(elem_str, hm, href_re)) {
            el.href = hm[1].str();
        }

        std::regex src_re(R"(src\s*=\s*["']([^"']+)["'])", std::regex::icase);
        std::smatch sm;
        if (std::regex_search(elem_str, sm, src_re)) {
            el.src = sm[1].str();
        }

        result.push_back(el);
    }
    return result;
}

std::vector<std::string> WebAutomation::extractLinks(const std::string& html) const {
    std::vector<std::string> links;
    std::regex link_re(R"(<a[^>]+href\s*=\s*["']([^"']+)["'][^>]*>)", std::regex::icase);
    std::sregex_iterator it(html.begin(), html.end(), link_re);
    std::sregex_iterator end;
    for (; it != end; ++it) {
        std::string href = (*it)[1].str();
        if (!href.empty() && href[0] != '#') {
            links.push_back(href);
        }
    }
    return links;
}

std::vector<std::string> WebAutomation::extractImages(const std::string& html) const {
    std::vector<std::string> images;
    std::regex img_re(R"(<img[^>]+src\s*=\s*["']([^"']+)["'][^>]*>)", std::regex::icase);
    std::sregex_iterator it(html.begin(), html.end(), img_re);
    std::sregex_iterator end;
    for (; it != end; ++it) {
        images.push_back((*it)[1].str());
    }
    return images;
}

std::string WebAutomation::normalizeUrl(const std::string& base, const std::string& href) const {
    if (href.find("://") != std::string::npos) return href;
    if (href.empty()) return base;

    std::string norm = base;
    size_t slash = base.find('/', base.find("://") + 3);
    if (slash != std::string::npos) {
        norm = base.substr(0, slash);
    }

    if (href[0] == '/') {
        return norm + href;
    }
    return norm + "/" + href;
}

void WebAutomation::setBrowserActionCallback(BrowserActionCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_browser_cb = std::move(cb);
}

std::string ScrapeResult::summary() const {
    if (!success) return "抓取失败: " + error;

    std::ostringstream oss;
    if (!full_text.empty()) {
        std::string preview = full_text.substr(0, std::min<size_t>(full_text.size(), 500));
        oss << "页面文本 (" << full_text.size() << " 字符):\n" << preview;
        if (full_text.size() > 500) oss << "...";
    }
    if (!links.empty()) {
        oss << "\n\n链接 (" << links.size() << " 个):";
        for (size_t i = 0; i < std::min<size_t>(links.size(), 20); ++i) {
            oss << "\n  " << links[i];
        }
        if (links.size() > 20) oss << "\n  ... 共 " << links.size() << " 个";
    }
    if (!images.empty()) {
        oss << "\n\n图片 (" << images.size() << " 个)";
    }
    return oss.str();
}

} // namespace npc