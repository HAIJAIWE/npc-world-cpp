#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <cstdint>

namespace npc {

struct ContextSlot {
    int64_t id;
    std::string key;
    std::string content;
    int64_t timestamp;
    int32_t tokenCount;
    int32_t importance;
};

class ContextManager {
public:
    static ContextManager& instance();

    void configure(int32_t maxTokens, int32_t reserveTokens, int32_t maxSlots);

    int32_t totalTokenCount() const;
    int32_t availableTokens() const;
    int32_t usedTokens() const;
    int32_t maxTokens() const { return m_maxTokens; }
    int32_t maxSlots() const { return m_maxSlots; }

    int64_t addSlot(const std::string& key, const std::string& content, int32_t importance = 1, int32_t tokenEstimate = -1);
    bool removeSlot(int64_t id);
    bool removeSlotByKey(const std::string& key);
    ContextSlot* getSlot(int64_t id);
    const ContextSlot* getSlot(int64_t id) const;
    ContextSlot* getSlotByKey(const std::string& key);
    const ContextSlot* getSlotByKey(const std::string& key) const;

    void clear();

    std::string buildPromptTemplate(
        const std::string& template_str,
        const std::vector<std::string>& slot_keys,
        const std::string& user_prompt) const;

    std::string mergeSlots(const std::vector<std::string>& keys, int32_t maxTokens) const;

    void trimToFit(int32_t targetTokens);
    int32_t estimateTokens(const std::string& text) const;

    struct SlotSnapshot {
        int64_t id;
        std::string key;
        int32_t tokenCount;
        int32_t importance;
        int64_t timestamp;
    };
    std::vector<SlotSnapshot> snapshot() const;

    class ScopedSlot {
    public:
        ScopedSlot(ContextManager& mgr, const std::string& key, const std::string& content, int32_t importance = 1);
        ~ScopedSlot();
        int64_t id() const { return m_id; }
    private:
        ContextManager& m_mgr;
        int64_t m_id;
    };

private:
    ContextManager() = default;
    ContextManager(const ContextManager&) = delete;
    ContextManager& operator=(const ContextManager&) = delete;

    void evictIfNeeded(int32_t neededTokens);
    void evictOne();

    mutable std::mutex m_mutex;
    int32_t m_maxTokens       = 6144;
    int32_t m_reserveTokens   = 1024;
    int32_t m_maxSlots        = 32;
    int64_t m_nextId          = 1;
    std::deque<ContextSlot> m_slots;
};

} // namespace npc
