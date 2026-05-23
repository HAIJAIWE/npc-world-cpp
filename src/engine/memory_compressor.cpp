#include "engine/memory_compressor.h"
#include "engine/npc_memory.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace npc {

MemoryCompressor& MemoryCompressor::instance() {
    static MemoryCompressor inst;
    return inst;
}

MemoryCompressor::MemoryCompressor() = default;

void MemoryCompressor::configure(const CompressionConfig& cfg) {
    m_config = cfg;
}

std::string MemoryCompressor::keywordExtract(const std::string& text, int max_chars) const {
    if ((int)text.size() <= max_chars) return text;

    std::string clean;
    clean.reserve(text.size());
    bool in_space = false;
    for (char c : text) {
        if (c == '\n' || c == '\r' || c == '\t') {
            if (!in_space) { clean += ' '; in_space = true; }
            continue;
        }
        in_space = false;
        clean += c;
    }

    if ((int)clean.size() <= max_chars) return clean;

    size_t cut = (size_t)max_chars;
    while (cut > max_chars / 2 && cut < clean.size() && clean[cut] != ' ') cut--;
    if (cut <= max_chars / 2) cut = max_chars;

    return clean.substr(0, cut) + "...";
}

std::string MemoryCompressor::generateSummary(const std::vector<std::string>& entries) const {
    std::ostringstream oss;
    oss << "[压缩摘要: " << entries.size() << " 条记忆]\n";

    std::vector<size_t> indices(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) indices[i] = i;

    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        auto score = [](const std::string& s) -> int {
            int s1 = 0;
            if (s.find('!') != std::string::npos) s1 += 3;
            if (s.find("重要") != std::string::npos) s1 += 2;
            if (s.find("必须") != std::string::npos) s1 += 2;
            return s1;
        };
        return score(entries[a]) > score(entries[b]);
    });

    for (size_t i = 0; i < std::min((size_t)m_config.compress_to, entries.size()); ++i) {
        oss << "  - " << keywordExtract(entries[indices[i]], m_config.summary_max_chars) << "\n";
    }

    return oss.str();
}

void MemoryCompressor::compress(const std::string& npc_id) {
    auto& store = NpcMemoryStore::instance();
    auto* bank = store.getBankPtr(npc_id);
    if (!bank) return;
    int short_count = (int)bank->short_term.size() + (int)bank->long_term.size();
    if (short_count < m_config.max_short_term) return;

    int to_remove = short_count - m_config.compress_to;
    if (to_remove <= 0) return;

    std::vector<std::string> entries;
    entries.reserve(to_remove);

    auto& st = bank->short_term;
    while ((int)entries.size() < to_remove && !st.empty()) {
        entries.push_back(st.front().content);
        st.pop_front();
        m_stats.entries_removed++;
        m_stats.chars_saved += (int64_t)entries.back().size();
    }

    auto& lt = bank->long_term;
    auto it = lt.begin();
    while ((int)entries.size() < to_remove && it != lt.end()) {
        if (it->importance < 0.3f) {
            entries.push_back(it->content);
            m_stats.entries_removed++;
            m_stats.chars_saved += (int64_t)it->content.size();
            it = lt.erase(it);
        } else {
            ++it;
        }
    }

    if (!entries.empty()) {
        std::string summary = generateSummary(entries);
        store.remember(npc_id, summary, 0.6f, 0.0f, "neutral", {}, {"summary"});
        m_stats.total_compressions++;
    }
}

} // namespace npc
