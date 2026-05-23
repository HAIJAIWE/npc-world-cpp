#include "llm_cache.h"
#include <algorithm>
#include <sstream>
#include <set>
#include <regex>
#include <cstdio>
#include <chrono>

LLMCache::LLMCache(size_t max_size, int64_t ttl_ms)
    : max_cache_size_(max_size), cache_ttl_ms_(ttl_ms) {}

uint32_t LLMCache::simple_hash(const std::string& str) {
    uint32_t h = 5381;
    for (char c : str) {
        h = (h * 33) ^ static_cast<uint32_t>(static_cast<unsigned char>(c));
    }
    return h;
}

uint32_t LLMCache::hash_prompt(const std::string& npc_id,
                                const std::string& situation,
                                const std::vector<std::string>& dialog_history) {
    std::string key = npc_id + "|";
    key += situation.substr(0, std::min<size_t>(100, situation.size())) + "|";

    if (!dialog_history.empty()) {
        int count = 0;
        for (auto it = dialog_history.rbegin(); it != dialog_history.rend() && count < 2; ++it, ++count) {
            if (count > 0) key += "|";
            key += it->substr(0, std::min<size_t>(200, it->size()));
        }
    }

    return simple_hash(key);
}

std::string LLMCache::try_get(const std::string& npc_id,
                               const std::string& situation,
                               const std::vector<std::string>& dialog_history) {
    uint32_t hash = hash_prompt(npc_id, situation, dialog_history);
    auto it = cache_.find(hash);

    if (it == cache_.end()) {
        miss_count_++;
        return "";
    }

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (now - it->second.timestamp > cache_ttl_ms_) {
        cache_.erase(it);
        miss_count_++;
        return "";
    }

    it->second.hit_count++;
    hit_count_++;

    return it->second.response;
}

void LLMCache::set(const std::string& npc_id,
                    const std::string& situation,
                    const std::vector<std::string>& dialog_history,
                    const std::string& response) {
    uint32_t hash = hash_prompt(npc_id, situation, dialog_history);

    if (cache_.count(hash)) return;

    if (cache_.size() >= max_cache_size_) {
        uint32_t oldest_key = 0;
        int64_t oldest_time = INT64_MAX;
        for (const auto& [k, v] : cache_) {
            if (v.timestamp < oldest_time) {
                oldest_time = v.timestamp;
                oldest_key = k;
            }
        }
        if (oldest_key != 0) cache_.erase(oldest_key);
    }

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    CacheEntry entry;
    entry.prompt_hash = std::to_string(hash);
    entry.response = response;
    entry.timestamp = now;
    entry.npc_id = npc_id;
    entry.hit_count = 0;

    cache_[hash] = std::move(entry);
}

static std::set<std::string> tokenize(const std::string& text) {
    std::set<std::string> words;
    std::regex word_re(R"([\u4e00-\u9fff]+|[a-zA-Z]+)");
    auto begin = std::sregex_iterator(text.begin(), text.end(), word_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        words.insert(it->str());
    }
    return words;
}

std::string LLMCache::find_similar(const std::string& npc_id,
                                    const std::string& situation,
                                    double threshold) {
    std::string sit = situation.substr(0, std::min<size_t>(200, situation.size()));
    auto situation_words = tokenize(sit);
    if (situation_words.empty()) return "";

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (auto& [hash_val, entry] : cache_) {
        if (entry.npc_id != npc_id) continue;
        if (now - entry.timestamp > cache_ttl_ms_) continue;

        std::string hash_str = entry.prompt_hash;
        size_t sep1 = hash_str.find('|');
        if (sep1 == std::string::npos) continue;
        size_t sep2 = hash_str.find('|', sep1 + 1);
        std::string cached_sit = (sep2 != std::string::npos)
            ? hash_str.substr(sep1 + 1, sep2 - sep1 - 1)
            : hash_str.substr(sep1 + 1);

        auto cached_words = tokenize(cached_sit);
        if (cached_words.empty()) continue;

        std::set<std::string> intersection;
        for (const auto& w : situation_words) {
            if (cached_words.count(w)) intersection.insert(w);
        }

        std::set<std::string> union_set = situation_words;
        union_set.insert(cached_words.begin(), cached_words.end());

        double similarity = static_cast<double>(intersection.size())
                          / static_cast<double>(std::max<size_t>(1, union_set.size()));

        if (similarity >= threshold) {
            entry.hit_count++;
            hit_count_++;
            return entry.response;
        }
    }

    return "";
}

std::string LLMCache::try_cache(const std::string& npc_id,
                                 const std::string& situation,
                                 const std::vector<std::string>& dialog_history) {
    std::string exact = try_get(npc_id, situation, dialog_history);
    if (!exact.empty()) return exact;

    return find_similar(npc_id, situation, 0.65);
}

CacheStats LLMCache::get_stats() const {
    CacheStats s;
    s.cache_size = cache_.size();
    s.max_size = max_cache_size_;
    s.hits = hit_count_;
    s.misses = miss_count_;
    int total = hit_count_ + miss_count_;
    s.hit_rate = total > 0 ? static_cast<double>(hit_count_) / total : 0.0;
    return s;
}

void LLMCache::clear() {
    cache_.clear();
    hit_count_ = 0;
    miss_count_ = 0;
}
