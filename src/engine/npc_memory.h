#pragma once

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <cstdint>

namespace npc {

enum class MemoryTier {
    ShortTerm,
    LongTerm,
    Core
};

struct MemoryEntry {
    int64_t id;
    std::string npc_id;
    std::string content;
    MemoryTier tier = MemoryTier::ShortTerm;
    int64_t timestamp = 0;
    float importance = 0.5f;
    float emotional_valence = 0.0f;
    std::string emotion_tag;
    std::vector<std::string> related_npcs;
    std::vector<std::string> tags;
    int access_count = 0;
    std::vector<float> embedding;
};

struct RetrievalResult {
    MemoryEntry memory;
    float relevance = 0.0f;
};

struct NpcMemoryStats {
    int short_term_count = 0;
    int long_term_count = 0;
    int core_count = 0;
    int total_memories = 0;
};

class NpcMemoryStore {
    friend class MemoryCompressor;
public:
    static constexpr int MAX_SHORT_TERM = 10;

    NpcMemoryStore()  = default;
    ~NpcMemoryStore() = default;

    void remember(const std::string& npc_id, const std::string& content,
                  float importance = 0.5f, float emotionalValence = 0.0f,
                  const std::string& emotionTag = "",
                  const std::vector<std::string>& relatedNpcs = {},
                  const std::vector<std::string>& tags = {});

    std::vector<RetrievalResult> retrieve(const std::string& npc_id,
                                           const std::string& query,
                                           int topK = 5,
                                           bool searchLongTerm = true);

    std::vector<RetrievalResult> retrieveByNpcs(const std::string& npc_id,
                                                 const std::vector<std::string>& npcIds,
                                                 int topK = 5);

    std::vector<RetrievalResult> retrieveByTags(const std::string& npc_id,
                                                 const std::vector<std::string>& tags,
                                                 int topK = 5);

    std::vector<RetrievalResult> retrieveRecent(const std::string& npc_id, int n = 5);

    void consolidate(const std::string& npc_id);

    void forgetOld(const std::string& npc_id, int maxAgeMinutes = 1440);

    void clearNpc(const std::string& npc_id);

    NpcMemoryStats getStats(const std::string& npc_id) const;

    std::string formatMemoriesForPrompt(const std::string& npc_id,
                                         const std::string& query,
                                         int maxTokens = 500);

    std::string formatRecentMemories(const std::string& npc_id, int maxTokens = 300);

    void updateImportance(const std::string& npc_id, int64_t memoryId,
                          float newImportance);

    void onNpcInteraction(const std::string& npcA, const std::string& npcB);

    std::vector<float> computeEmbedding(const std::string& text) const;

    static NpcMemoryStore& instance();

private:
    NpcMemoryStore(const NpcMemoryStore&) = delete;
    NpcMemoryStore& operator=(const NpcMemoryStore&) = delete;

    float cosineSimilarity(const std::vector<float>& a,
                           const std::vector<float>& b) const;
    float keywordRelevance(const MemoryEntry& mem,
                           const std::string& query) const;
    int64_t nowMs() const;

    int64_t m_nextId = 1;

    struct NpcMemoryBank {
        std::deque<MemoryEntry> short_term;
        std::vector<MemoryEntry> long_term;
        std::vector<MemoryEntry> core;
    };
    NpcMemoryBank* getBankPtr(const std::string& npc_id);
    void setBankFrom(const std::string& npc_id, const NpcMemoryBank& src);

    std::unordered_map<std::string, NpcMemoryBank> m_banks;
    mutable std::mutex m_mutex;
};

} // namespace npc
