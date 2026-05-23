#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <functional>

struct CacheEntry {
    std::string prompt_hash;
    std::string response;
    int64_t timestamp = 0;
    std::string npc_id;
    int hit_count = 0;
};

struct CacheStats {
    size_t cache_size = 0;
    size_t max_size = 0;
    int hits = 0;
    int misses = 0;
    double hit_rate = 0.0;
};

class LLMCache {
public:
    LLMCache(size_t max_size = 2000, int64_t ttl_ms = 21600000);
    ~LLMCache() = default;

    std::string try_get(const std::string& npc_id,
                        const std::string& situation,
                        const std::vector<std::string>& dialog_history);

    void set(const std::string& npc_id,
             const std::string& situation,
             const std::vector<std::string>& dialog_history,
             const std::string& response);

    std::string try_cache(const std::string& npc_id,
                          const std::string& situation,
                          const std::vector<std::string>& dialog_history);

    CacheStats get_stats() const;

    void clear();

    static uint32_t hash_prompt(const std::string& npc_id,
                                const std::string& situation,
                                const std::vector<std::string>& dialog_history);

private:
    std::string find_similar(const std::string& npc_id,
                             const std::string& situation,
                             double threshold = 0.75);

    static uint32_t simple_hash(const std::string& str);

    std::unordered_map<uint32_t, CacheEntry> cache_;
    size_t max_cache_size_;
    int64_t cache_ttl_ms_;
    int hit_count_ = 0;
    int miss_count_ = 0;
};
