#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace npc {

struct MemoryEntry;
class NpcMemoryStore;

struct CompressionConfig {
    int max_short_term = 12;
    int compress_to = 3;
    int summary_max_chars = 256;
    bool use_llm_for_summary = false;
};

class MemoryCompressor {
public:
    static MemoryCompressor& instance();

    void configure(const CompressionConfig& cfg);
    void compress(const std::string& npc_id);

    std::string generateSummary(const std::vector<std::string>& entries) const;
    std::string keywordExtract(const std::string& text, int max_chars) const;

    struct CompressStats {
        int64_t total_compressions = 0;
        int64_t entries_removed = 0;
        int64_t chars_saved = 0;
    };
    CompressStats stats() const { return m_stats; }

private:
    MemoryCompressor();
    CompressionConfig m_config;
    CompressStats m_stats;
};

} // namespace npc
