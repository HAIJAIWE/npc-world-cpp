#include "engine/npc_memory.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <sstream>
#include <set>
#include <regex>

namespace npc {

static constexpr int EMBED_DIM = 128;

NpcMemoryStore& NpcMemoryStore::instance() {
    static NpcMemoryStore store;
    return store;
}

int64_t NpcMemoryStore::nowMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static std::vector<std::string> tokenizeCjk(const std::string& text) {
    std::vector<std::string> result;
    std::string current;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            current.clear();
            while (i < text.size() && static_cast<unsigned char>(text[i]) < 0x80) {
                char ch = text[i];
                if (ch == ' ' || ch == '\n' || ch == '\t' || ch == ',' || ch == '.' ||
                    ch == '!' || ch == '?' || ch == ';' || ch == ':') {
                    if (!current.empty()) { result.push_back(current); current.clear(); }
                } else {
                    current += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                }
                i++;
            }
            if (!current.empty()) { result.push_back(current); current.clear(); }
        } else {
            if (!current.empty()) { result.push_back(current); current.clear(); }
            int bytes = 0;
            if ((c & 0xE0) == 0xC0) bytes = 2;
            else if ((c & 0xF0) == 0xE0) bytes = 3;
            else if ((c & 0xF8) == 0xF0) bytes = 4;
            else { i++; continue; }
            if (i + bytes <= text.size()) {
                result.push_back(text.substr(i, bytes));
            }
            i += bytes;
        }
    }
    if (!current.empty()) result.push_back(current);
    return result;
}

std::vector<float> NpcMemoryStore::computeEmbedding(const std::string& text) const {
    std::vector<float> emb(EMBED_DIM, 0.0f);
    auto tokens = tokenizeCjk(text);
    if (tokens.empty()) return emb;

    std::vector<std::string> bigrams;
    for (size_t i = 0; i + 1 < tokens.size(); i++) {
        bigrams.push_back(tokens[i] + "_" + tokens[i + 1]);
    }
    for (const auto& t : tokens) {
        bigrams.push_back(t);
    }

    float denom = std::sqrt(static_cast<float>(bigrams.size())) + 1.0f;
    for (const auto& bg : bigrams) {
        uint32_t h = 5381;
        for (char c : bg) { h = ((h << 5) + h) + static_cast<unsigned char>(c); }
        size_t idx = h % EMBED_DIM;
        emb[idx] += 1.0f / denom;
    }

    float norm = 0.0f;
    for (float v : emb) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-8f) {
        for (float& v : emb) v /= norm;
    }
    return emb;
}

float NpcMemoryStore::cosineSimilarity(const std::vector<float>& a,
                                        const std::vector<float>& b) const {
    if (a.empty() || b.empty()) return 0.0f;
    float dot = 0.0f, normA = 0.0f, normB = 0.0f;
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; i++) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    if (normA < 1e-8f || normB < 1e-8f) return 0.0f;
    return dot / (std::sqrt(normA) * std::sqrt(normB));
}

float NpcMemoryStore::keywordRelevance(const MemoryEntry& mem,
                                        const std::string& query) const {
    auto queryTokens = tokenizeCjk(query);
    auto memTokens = tokenizeCjk(mem.content);
    if (queryTokens.empty() || memTokens.empty()) return 0.0f;

    std::set<std::string> querySet(queryTokens.begin(), queryTokens.end());
    std::set<std::string> memSet(memTokens.begin(), memTokens.end());

    int intersect = 0;
    for (const auto& t : querySet) {
        if (memSet.count(t)) intersect++;
    }
    int unionSize = static_cast<int>(querySet.size() + memSet.size() - intersect);
    float jaccard = unionSize > 0 ? static_cast<float>(intersect) / unionSize : 0.0f;

    float recency = 0.0f;
    int64_t age = nowMs() - mem.timestamp;
    recency = std::exp(-static_cast<float>(age) / (3600000.0f * 24.0f));

    return 0.4f * jaccard + 0.3f * mem.importance + 0.2f * recency +
           0.1f * (mem.access_count / 10.0f);
}

void NpcMemoryStore::remember(const std::string& npc_id, const std::string& content,
                               float importance, float emotionalValence,
                               const std::string& emotionTag,
                               const std::vector<std::string>& relatedNpcs,
                               const std::vector<std::string>& tags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& bank = m_banks[npc_id];

    MemoryEntry entry;
    entry.id = m_nextId++;
    entry.npc_id = npc_id;
    entry.content = content;
    entry.tier = MemoryTier::ShortTerm;
    entry.timestamp = nowMs();
    entry.importance = importance;
    entry.emotional_valence = emotionalValence;
    entry.emotion_tag = emotionTag;
    entry.related_npcs = relatedNpcs;
    entry.tags = tags;
    entry.embedding = computeEmbedding(content);

    bank.short_term.push_back(entry);

    while (static_cast<int>(bank.short_term.size()) > MAX_SHORT_TERM) {
        MemoryEntry evicted = bank.short_term.front();
        bank.short_term.pop_front();
        if (evicted.importance >= 0.3f) {
            evicted.tier = MemoryTier::LongTerm;
            bank.long_term.push_back(std::move(evicted));
        }
    }

    if (bank.long_term.size() > 500) {
        std::sort(bank.long_term.begin(), bank.long_term.end(),
            [](const MemoryEntry& a, const MemoryEntry& b) {
                return (a.importance * 0.7f + a.access_count * 0.01f) >
                       (b.importance * 0.7f + b.access_count * 0.01f);
            });
        bank.long_term.resize(500);
    }
}

std::vector<RetrievalResult> NpcMemoryStore::retrieve(const std::string& npc_id,
                                                       const std::string& query,
                                                       int topK, bool searchLongTerm) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_banks.find(npc_id);
    if (it == m_banks.end()) return {};

    auto& bank = it->second;
    auto queryEmb = computeEmbedding(query);

    struct ScoredEntry {
        MemoryEntry* mem;
        float score;
    };
    std::vector<ScoredEntry> candidates;

    for (auto& mem : bank.short_term) {
        float kwRel = keywordRelevance(mem, query);
        float embSim = cosineSimilarity(queryEmb, mem.embedding);
        float score = 0.5f * kwRel + 0.5f * embSim;
        candidates.push_back({&mem, score});
    }

    if (searchLongTerm) {
        for (auto& mem : bank.long_term) {
            float kwRel = keywordRelevance(mem, query);
            float embSim = cosineSimilarity(queryEmb, mem.embedding);
            float score = 0.3f * kwRel + 0.7f * embSim;
            candidates.push_back({&mem, score});
        }
        for (auto& mem : bank.core) {
            float kwRel = keywordRelevance(mem, query);
            float embSim = cosineSimilarity(queryEmb, mem.embedding);
            float score = 0.2f * kwRel + 0.8f * embSim;
            candidates.push_back({&mem, score});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const ScoredEntry& a, const ScoredEntry& b) { return a.score > b.score; });

    std::vector<RetrievalResult> results;
    for (int i = 0; i < std::min(topK, static_cast<int>(candidates.size())); i++) {
        candidates[i].mem->access_count++;
        results.push_back({*candidates[i].mem, candidates[i].score});
    }
    return results;
}

std::vector<RetrievalResult> NpcMemoryStore::retrieveByNpcs(const std::string& npc_id,
                                                             const std::vector<std::string>& npcIds,
                                                             int topK) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_banks.find(npc_id);
    if (it == m_banks.end()) return {};

    auto& bank = it->second;
    std::set<std::string> targetSet(npcIds.begin(), npcIds.end());

    struct ScoredEntry {
        MemoryEntry* mem;
        float score;
    };
    std::vector<ScoredEntry> candidates;

    auto collect = [&](auto& container) {
        for (auto& mem : container) {
            int matchCount = 0;
            for (const auto& r : mem.related_npcs) {
                if (targetSet.count(r)) matchCount++;
            }
            if (matchCount > 0) {
                float recency = std::exp(
                    -static_cast<float>(nowMs() - mem.timestamp) / (3600000.0f * 48.0f));
                candidates.push_back({&mem, matchCount * mem.importance * recency});
            }
        }
    };

    collect(bank.short_term);
    collect(bank.long_term);
    collect(bank.core);

    std::sort(candidates.begin(), candidates.end(),
        [](const ScoredEntry& a, const ScoredEntry& b) { return a.score > b.score; });

    std::vector<RetrievalResult> results;
    for (int i = 0; i < std::min(topK, static_cast<int>(candidates.size())); i++) {
        results.push_back({*candidates[i].mem, candidates[i].score});
    }
    return results;
}

std::vector<RetrievalResult> NpcMemoryStore::retrieveByTags(const std::string& npc_id,
                                                             const std::vector<std::string>& tags,
                                                             int topK) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_banks.find(npc_id);
    if (it == m_banks.end()) return {};

    auto& bank = it->second;
    std::set<std::string> tagSet(tags.begin(), tags.end());

    struct ScoredEntry {
        MemoryEntry* mem;
        float score;
    };
    std::vector<ScoredEntry> candidates;

    auto collect = [&](auto& container) {
        for (auto& mem : container) {
            int matchCount = 0;
            for (const auto& t : mem.tags) {
                if (tagSet.count(t)) matchCount++;
            }
            if (matchCount > 0) {
                candidates.push_back({&mem,
                    matchCount * mem.importance * 0.01f * mem.access_count});
            }
        }
    };

    collect(bank.short_term);
    collect(bank.long_term);
    collect(bank.core);

    std::sort(candidates.begin(), candidates.end(),
        [](const ScoredEntry& a, const ScoredEntry& b) { return a.score > b.score; });

    std::vector<RetrievalResult> results;
    for (int i = 0; i < std::min(topK, static_cast<int>(candidates.size())); i++) {
        results.push_back({*candidates[i].mem, candidates[i].score});
    }
    return results;
}

std::vector<RetrievalResult> NpcMemoryStore::retrieveRecent(const std::string& npc_id, int n) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_banks.find(npc_id);
    if (it == m_banks.end()) return {};

    auto& bank = it->second;
    std::vector<RetrievalResult> results;
    int count = 0;
    for (auto rit = bank.short_term.rbegin();
         rit != bank.short_term.rend() && count < n; ++rit, ++count) {
        results.push_back({*rit, 1.0f});
    }
    return results;
}

void NpcMemoryStore::consolidate(const std::string& npc_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_banks.find(npc_id);
    if (it == m_banks.end()) return;

    auto& bank = it->second;
    std::vector<MemoryEntry*> promoted;
    for (auto& mem : bank.short_term) {
        if (mem.importance >= 0.6f && mem.access_count >= 3) {
            mem.tier = MemoryTier::LongTerm;
            promoted.push_back(&mem);
        }
    }
    for (auto& mem : bank.long_term) {
        if (mem.importance >= 0.8f && mem.access_count >= 5) {
            mem.tier = MemoryTier::Core;
            bank.core.push_back(std::move(mem));
        }
    }
    bank.long_term.erase(
        std::remove_if(bank.long_term.begin(), bank.long_term.end(),
                       [](const MemoryEntry& m) { return m.content.empty(); }),
        bank.long_term.end());

    fprintf(stdout, "[NpcMemory] Promoted %zu to long-term, core size=%zu\n",
        promoted.size(), bank.core.size());
}

void NpcMemoryStore::forgetOld(const std::string& npc_id, int maxAgeMinutes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_banks.find(npc_id);
    if (it == m_banks.end()) return;

    auto& bank = it->second;
    int64_t cutoff = nowMs() - static_cast<int64_t>(maxAgeMinutes) * 60000;

    auto isOld = [cutoff](const MemoryEntry& m) { return m.timestamp < cutoff; };
    bank.short_term.erase(
        std::remove_if(bank.short_term.begin(), bank.short_term.end(), isOld),
        bank.short_term.end());
    bank.long_term.erase(
        std::remove_if(bank.long_term.begin(), bank.long_term.end(), isOld),
        bank.long_term.end());
}

void NpcMemoryStore::clearNpc(const std::string& npc_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_banks.erase(npc_id);
}

NpcMemoryStats NpcMemoryStore::getStats(const std::string& npc_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_banks.find(npc_id);
    if (it == m_banks.end()) return {};

    auto& bank = it->second;
    NpcMemoryStats stats;
    stats.short_term_count = static_cast<int>(bank.short_term.size());
    stats.long_term_count  = static_cast<int>(bank.long_term.size());
    stats.core_count       = static_cast<int>(bank.core.size());
    stats.total_memories   = stats.short_term_count + stats.long_term_count + stats.core_count;
    return stats;
}

std::string NpcMemoryStore::formatMemoriesForPrompt(const std::string& npc_id,
                                                      const std::string& query,
                                                      int maxTokens) {
    auto results = retrieve(npc_id, query, 8, true);
    std::ostringstream oss;
    int tokenEstimate = 0;
    for (const auto& r : results) {
        int est = static_cast<int>(r.memory.content.size() / 3);
        if (tokenEstimate + est > maxTokens) break;
        oss << "- " << r.memory.content << " [" << r.memory.emotion_tag << "]\n";
        tokenEstimate += est;
    }
    return oss.str();
}

std::string NpcMemoryStore::formatRecentMemories(const std::string& npc_id,
                                                   int maxTokens) {
    auto results = retrieveRecent(npc_id, 10);
    std::ostringstream oss;
    int tokenEstimate = 0;
    for (const auto& r : results) {
        int est = static_cast<int>(r.memory.content.size() / 3);
        if (tokenEstimate + est > maxTokens) break;
        oss << "- " << r.memory.content << "\n";
        tokenEstimate += est;
    }
    return oss.str();
}

void NpcMemoryStore::updateImportance(const std::string& npc_id,
                                       int64_t memoryId, float newImportance) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_banks.find(npc_id);
    if (it == m_banks.end()) return;

    auto update = [memoryId, newImportance](auto& container) {
        for (auto& mem : container) {
            if (mem.id == memoryId) { mem.importance = newImportance; return true; }
        }
        return false;
    };
    if (update(it->second.short_term)) return;
    if (update(it->second.long_term)) return;
    update(it->second.core);
}

void NpcMemoryStore::onNpcInteraction(const std::string& npcA,
                                       const std::string& npcB) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto markInteraction = [](NpcMemoryBank& bank, const std::string& otherNpc) {
        for (auto& mem : bank.short_term) {
            for (const auto& r : mem.related_npcs) {
                if (r == otherNpc) mem.access_count++;
            }
        }
    };

    auto itA = m_banks.find(npcA);
    if (itA != m_banks.end()) markInteraction(itA->second, npcB);

    auto itB = m_banks.find(npcB);
    if (itB != m_banks.end()) markInteraction(itB->second, npcA);
}

NpcMemoryStore::NpcMemoryBank* NpcMemoryStore::getBankPtr(const std::string& npc_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_banks.find(npc_id);
    if (it == m_banks.end()) return nullptr;
    return &it->second;
}

void NpcMemoryStore::setBankFrom(const std::string& npc_id, const NpcMemoryBank& src) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_banks[npc_id] = src;
}

} // namespace npc
