#include "engine/context_manager.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <ctime>

namespace npc {

ContextManager& ContextManager::instance() {
    static ContextManager mgr;
    return mgr;
}

void ContextManager::configure(int32_t maxTokens, int32_t reserveTokens, int32_t maxSlots) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxTokens = maxTokens;
    m_reserveTokens = reserveTokens;
    m_maxSlots = maxSlots;
}

int32_t ContextManager::estimateTokens(const std::string& text) const {
    int32_t chars = static_cast<int32_t>(text.size());
    int32_t tokens = static_cast<int32_t>(std::ceil(chars / 3.5));
    tokens += static_cast<int32_t>(std::ceil(chars / 50.0));
    return std::max(1, tokens);
}

int32_t ContextManager::totalTokenCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int32_t total = 0;
    for (const auto& slot : m_slots) {
        total += slot.tokenCount > 0 ? slot.tokenCount : estimateTokens(slot.content);
    }
    return total;
}

int32_t ContextManager::usedTokens() const {
    return totalTokenCount();
}

int32_t ContextManager::availableTokens() const {
    return std::max(0, m_maxTokens - m_reserveTokens - usedTokens());
}

int64_t ContextManager::addSlot(const std::string& key, const std::string& content, int32_t importance, int32_t tokenEstimate) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& slot : m_slots) {
        if (slot.key == key) {
            slot.content = content;
            slot.timestamp = static_cast<int64_t>(std::time(nullptr));
            slot.importance = importance;
            slot.tokenCount = tokenEstimate > 0 ? tokenEstimate : estimateTokens(content);
            return slot.id;
        }
    }

    int32_t tokens = tokenEstimate > 0 ? tokenEstimate : estimateTokens(content);
    evictIfNeeded(tokens);

    if (static_cast<int32_t>(m_slots.size()) >= m_maxSlots) {
        evictOne();
    }

    ContextSlot slot;
    slot.id = m_nextId++;
    slot.key = key;
    slot.content = content;
    slot.timestamp = static_cast<int64_t>(std::time(nullptr));
    slot.importance = importance;
    slot.tokenCount = tokens;

    m_slots.push_back(slot);
    return slot.id;
}

bool ContextManager::removeSlot(int64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_slots.begin(), m_slots.end(),
        [id](const ContextSlot& s) { return s.id == id; });
    if (it != m_slots.end()) {
        m_slots.erase(it);
        return true;
    }
    return false;
}

bool ContextManager::removeSlotByKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_slots.begin(), m_slots.end(),
        [&key](const ContextSlot& s) { return s.key == key; });
    if (it != m_slots.end()) {
        m_slots.erase(it);
        return true;
    }
    return false;
}

ContextSlot* ContextManager::getSlot(int64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& slot : m_slots) {
        if (slot.id == id) return &slot;
    }
    return nullptr;
}

const ContextSlot* ContextManager::getSlot(int64_t id) const {
    auto it = std::find_if(m_slots.begin(), m_slots.end(),
        [id](const ContextSlot& s) { return s.id == id; });
    return it != m_slots.end() ? &(*it) : nullptr;
}

ContextSlot* ContextManager::getSlotByKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& slot : m_slots) {
        if (slot.key == key) return &slot;
    }
    return nullptr;
}

const ContextSlot* ContextManager::getSlotByKey(const std::string& key) const {
    auto it = std::find_if(m_slots.begin(), m_slots.end(),
        [&key](const ContextSlot& s) { return s.key == key; });
    return it != m_slots.end() ? &(*it) : nullptr;
}

void ContextManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_slots.clear();
}

std::string ContextManager::buildPromptTemplate(
    const std::string& template_str,
    const std::vector<std::string>& slot_keys,
    const std::string& user_prompt) const
{
    std::ostringstream result;
    size_t pos = 0;
    size_t lastPos = 0;

    while ((pos = template_str.find("{{", lastPos)) != std::string::npos) {
        result << template_str.substr(lastPos, pos - lastPos);
        size_t endPos = template_str.find("}}", pos + 2);
        if (endPos == std::string::npos) break;

        std::string key = template_str.substr(pos + 2, endPos - pos - 2);
        const ContextSlot* slot = getSlotByKey(key);
        if (slot) {
            result << slot->content;
        } else if (key == "user_prompt") {
            result << user_prompt;
        }

        lastPos = endPos + 2;
    }
    result << template_str.substr(lastPos);

    return result.str();
}

std::string ContextManager::mergeSlots(const std::vector<std::string>& keys, int32_t maxTokens) const {
    std::ostringstream result;
    int32_t tokenBudget = maxTokens;

    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& key : keys) {
        const ContextSlot* slot = getSlotByKey(key);
        if (!slot) continue;

        if (tokenBudget <= 0) break;

        int32_t slotTokens = slot->tokenCount > 0 ? slot->tokenCount : estimateTokens(slot->content);
        if (slotTokens > tokenBudget) {
            int32_t maxChars = std::max(1, static_cast<int32_t>(tokenBudget * 3.5));
            std::string truncated = slot->content.substr(0, maxChars) + "...";
            result << truncated << "\n";
            tokenBudget = 0;
        } else {
            result << slot->content << "\n";
            tokenBudget -= slotTokens;
        }
    }

    return result.str();
}

void ContextManager::trimToFit(int32_t targetTokens) {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (usedTokens() > targetTokens && !m_slots.empty()) {
        evictOne();
    }
}

void ContextManager::evictIfNeeded(int32_t neededTokens) {
    int32_t current = usedTokens();
    int32_t limit = m_maxTokens - m_reserveTokens;
    while (current + neededTokens > limit && !m_slots.empty()) {
        evictOne();
        current = usedTokens();
    }
}

void ContextManager::evictOne() {
    if (m_slots.empty()) return;

    auto oldest = std::min_element(m_slots.begin(), m_slots.end(),
        [](const ContextSlot& a, const ContextSlot& b) {
            int64_t scoreA = a.timestamp - a.importance * 1000;
            int64_t scoreB = b.timestamp - b.importance * 1000;
            return scoreA < scoreB;
        });

    m_slots.erase(oldest);
}

std::vector<ContextManager::SlotSnapshot> ContextManager::snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SlotSnapshot> result;
    result.reserve(m_slots.size());
    for (const auto& s : m_slots) {
        result.push_back({s.id, s.key, s.tokenCount, s.importance, s.timestamp});
    }
    return result;
}

ContextManager::ScopedSlot::ScopedSlot(ContextManager& mgr, const std::string& key,
                                        const std::string& content, int32_t importance)
    : m_mgr(mgr)
{
    m_id = m_mgr.addSlot(key, content, importance);
}

ContextManager::ScopedSlot::~ScopedSlot() {
    m_mgr.removeSlot(m_id);
}

} // namespace npc
