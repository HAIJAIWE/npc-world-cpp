#include "../../include/behavior/npc_brain.h"
#include "../../include/behavior/npc_emotion.h"
#include "../../include/common/npc_types.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace npc {

NPCBrainManager::NPCBrainManager()
    : m_nextMemoryId(0)
{
}

NPCBrainManager::~NPCBrainManager()
{
}

void NPCBrainManager::Initialize(const NPCBrainConfig& config)
{
    m_config = config;
    m_nextMemoryId = 0;
    
    m_shortTermMemories.clear();
    m_longTermMemories.clear();
    m_coreMemories.clear();
    m_personalityTraits.clear();
    m_emotions.clear();
    m_worldview.clear();
    m_moodHistory.clear();
    m_goalQueue.clear();
    m_knownNPCs.clear();
    m_knowledgeBase.clear();
    m_interactionCounts.clear();
    m_dailySummary.clear();
    m_nightlyConsolidations.clear();
    m_lastConsolidationTime = 0;
    
    bool hasConfigTraits = false;
    for (const auto& trait : config.personalityTraits) {
        if (!trait.name.empty()) {
            hasConfigTraits = true;
            break;
        }
    }
    
    if (!hasConfigTraits) {
        InitializeDefaultTraits(config.npcType);
    } else {
        m_personalityTraits = config.personalityTraits;
    }
    
    for (const auto& emotion : config.initialEmotions) {
        if (!emotion.name.empty()) {
            m_emotions.push_back(emotion);
        }
    }
    
    for (const auto& wv : config.initialWorldview) {
        if (!wv.empty()) {
            m_worldview.push_back(wv);
        }
    }
    
    for (const auto& memory : config.initialMemories) {
        if (!memory.content.empty()) {
            AddMemory(memory);
        }
    }
    
    for (const auto& goal : config.initialGoals) {
        if (!goal.description.empty()) {
            m_goalQueue.push_back(goal);
        }
    }
}

void NPCBrainManager::InitializeDefaultTraits(NPCType type)
{
    m_personalityTraits.clear();
    
    switch (type) {
        case NPCType::Ordinary:
            m_personalityTraits.push_back({"openness", 0.5f, 0.7f});
            m_personalityTraits.push_back({"conscientiousness", 0.5f, 0.7f});
            m_personalityTraits.push_back({"extraversion", 0.3f, 0.8f});
            m_personalityTraits.push_back({"agreeableness", 0.5f, 0.8f});
            m_personalityTraits.push_back({"neuroticism", 0.2f, 0.6f});
            break;
        case NPCType::Awakened:
            m_personalityTraits.push_back({"openness", 0.6f, 0.9f});
            m_personalityTraits.push_back({"conscientiousness", 0.5f, 0.8f});
            m_personalityTraits.push_back({"extraversion", 0.3f, 0.7f});
            m_personalityTraits.push_back({"agreeableness", 0.4f, 0.7f});
            m_personalityTraits.push_back({"neuroticism", 0.4f, 0.8f});
            break;
        case NPCType::RuthlessRuler:
            m_personalityTraits.push_back({"openness", 0.2f, 0.5f});
            m_personalityTraits.push_back({"conscientiousness", 0.7f, 1.0f});
            m_personalityTraits.push_back({"extraversion", 0.6f, 1.0f});
            m_personalityTraits.push_back({"agreeableness", 0.0f, 0.3f});
            m_personalityTraits.push_back({"neuroticism", 0.0f, 0.3f});
            break;
        case NPCType::IdealistMartyr:
            m_personalityTraits.push_back({"openness", 0.7f, 1.0f});
            m_personalityTraits.push_back({"conscientiousness", 0.6f, 1.0f});
            m_personalityTraits.push_back({"extraversion", 0.4f, 0.7f});
            m_personalityTraits.push_back({"agreeableness", 0.7f, 1.0f});
            m_personalityTraits.push_back({"neuroticism", 0.3f, 0.6f});
            break;
        case NPCType::ReligiousFanatic:
            m_personalityTraits.push_back({"openness", 0.0f, 0.2f});
            m_personalityTraits.push_back({"conscientiousness", 0.4f, 0.8f});
            m_personalityTraits.push_back({"extraversion", 0.5f, 0.9f});
            m_personalityTraits.push_back({"agreeableness", 0.1f, 0.4f});
            m_personalityTraits.push_back({"neuroticism", 0.7f, 1.0f});
            break;
        case NPCType::ManipulativeStrategist:
            m_personalityTraits.push_back({"openness", 0.6f, 0.9f});
            m_personalityTraits.push_back({"conscientiousness", 0.5f, 0.9f});
            m_personalityTraits.push_back({"extraversion", 0.4f, 0.8f});
            m_personalityTraits.push_back({"agreeableness", 0.0f, 0.3f});
            m_personalityTraits.push_back({"neuroticism", 0.1f, 0.4f});
            break;
        case NPCType::RebelliousFreeSpirit:
            m_personalityTraits.push_back({"openness", 0.8f, 1.0f});
            m_personalityTraits.push_back({"conscientiousness", 0.0f, 0.3f});
            m_personalityTraits.push_back({"extraversion", 0.6f, 1.0f});
            m_personalityTraits.push_back({"agreeableness", 0.3f, 0.6f});
            m_personalityTraits.push_back({"neuroticism", 0.4f, 0.7f});
            break;
        case NPCType::ConservativeGuardian:
            m_personalityTraits.push_back({"openness", 0.0f, 0.3f});
            m_personalityTraits.push_back({"conscientiousness", 0.7f, 1.0f});
            m_personalityTraits.push_back({"extraversion", 0.2f, 0.5f});
            m_personalityTraits.push_back({"agreeableness", 0.4f, 0.7f});
            m_personalityTraits.push_back({"neuroticism", 0.2f, 0.5f});
            break;
        case NPCType::AdventurousExplorer:
            m_personalityTraits.push_back({"openness", 0.8f, 1.0f});
            m_personalityTraits.push_back({"conscientiousness", 0.2f, 0.5f});
            m_personalityTraits.push_back({"extraversion", 0.5f, 0.9f});
            m_personalityTraits.push_back({"agreeableness", 0.5f, 0.8f});
            m_personalityTraits.push_back({"neuroticism", 0.0f, 0.3f});
            break;
        case NPCType::MysteriousStranger:
            m_personalityTraits.push_back({"openness", 0.5f, 0.8f});
            m_personalityTraits.push_back({"conscientiousness", 0.3f, 0.6f});
            m_personalityTraits.push_back({"extraversion", 0.1f, 0.4f});
            m_personalityTraits.push_back({"agreeableness", 0.3f, 0.6f});
            m_personalityTraits.push_back({"neuroticism", 0.5f, 0.8f});
            break;
        case NPCType::CorruptBureaucrat:
            m_personalityTraits.push_back({"openness", 0.1f, 0.4f});
            m_personalityTraits.push_back({"conscientiousness", 0.3f, 0.6f});
            m_personalityTraits.push_back({"extraversion", 0.4f, 0.7f});
            m_personalityTraits.push_back({"agreeableness", 0.1f, 0.4f});
            m_personalityTraits.push_back({"neuroticism", 0.2f, 0.5f});
            break;
        case NPCType::NurturingHealer:
            m_personalityTraits.push_back({"openness", 0.4f, 0.7f});
            m_personalityTraits.push_back({"conscientiousness", 0.5f, 0.8f});
            m_personalityTraits.push_back({"extraversion", 0.3f, 0.6f});
            m_personalityTraits.push_back({"agreeableness", 0.8f, 1.0f});
            m_personalityTraits.push_back({"neuroticism", 0.1f, 0.4f});
            break;
        default:
            m_personalityTraits.push_back({"openness", 0.4f, 0.6f});
            m_personalityTraits.push_back({"conscientiousness", 0.4f, 0.6f});
            m_personalityTraits.push_back({"extraversion", 0.4f, 0.6f});
            m_personalityTraits.push_back({"agreeableness", 0.4f, 0.6f});
            m_personalityTraits.push_back({"neuroticism", 0.4f, 0.6f});
            break;
    }
}

void NPCBrainManager::Update(float deltaSeconds, int64_t gameTime)
{
    UpdateEmotionDecay(deltaSeconds);
    
    ConsolidateMemories(gameTime);
    
    PruneMemories();
    
    if (gameTime - m_lastConsolidationTime > 86400) {
        NightlyConsolidation(gameTime);
        m_lastConsolidationTime = gameTime;
    }
}

void NPCBrainManager::AddMemory(const NPCBrainMemory& memory)
{
    NPCBrainMemory mem = memory;
    mem.id = ++m_nextMemoryId;
    mem.timestamp = memory.timestamp > 0 ? memory.timestamp : static_cast<int64_t>(time(nullptr));
    
    if (mem.importance > 0.7f) {
        mem.memoryType = MemoryType::Core;
        m_coreMemories.push_back(mem);
    } else if (mem.importance > 0.4f) {
        mem.memoryType = MemoryType::LongTerm;
        m_longTermMemories.push_back(mem);
    } else {
        mem.memoryType = MemoryType::ShortTerm;
        m_shortTermMemories.push_back(mem);
    }
    
    if (!mem.emotion.empty()) {
        MoodHistoryEntry entry;
        entry.emotionName = mem.emotion;
        entry.intensity = mem.importance;
        entry.timestamp = mem.timestamp;
        entry.trigger = mem.content;
        m_moodHistory.push_back(entry);
    }
}

std::vector<NPCBrainMemory> NPCBrainManager::GetRelevantMemories(
    const std::string& context, 
    int maxCount) const
{
    std::vector<NPCBrainMemory> relevant;
    
    auto addRelevant = [&](const std::vector<NPCBrainMemory>& memories) {
        for (const auto& mem : memories) {
            if (static_cast<int>(relevant.size()) >= maxCount) return;
            
            float relevance = CalculateRelevance(mem, context);
            if (relevance > 0.3f) {
                relevant.push_back(mem);
            }
        }
    };
    
    addRelevant(m_shortTermMemories);
    addRelevant(m_longTermMemories);
    addRelevant(m_coreMemories);
    
    std::sort(relevant.begin(), relevant.end(),
              [](const NPCBrainMemory& a, const NPCBrainMemory& b) {
                  return a.timestamp > b.timestamp;
              });
    
    if (static_cast<int>(relevant.size()) > maxCount) {
        relevant.resize(maxCount);
    }
    
    return relevant;
}

float NPCBrainManager::CalculateRelevance(
    const NPCBrainMemory& memory, 
    const std::string& context) const
{
    float relevance = 0.0f;
    
    std::string ctxLower = context;
    std::string contentLower = memory.content;
    std::string emotionLower = memory.emotion;
    std::transform(ctxLower.begin(), ctxLower.end(), ctxLower.begin(), ::tolower);
    std::transform(contentLower.begin(), contentLower.end(), contentLower.begin(), ::tolower);
    std::transform(emotionLower.begin(), emotionLower.end(), emotionLower.begin(), ::tolower);
    
    if (contentLower.find(ctxLower) != std::string::npos ||
        ctxLower.find(contentLower) != std::string::npos) {
        relevance += 0.5f;
    }
    
    relevance += memory.importance * 0.3f;
    
    int64_t now = static_cast<int64_t>(time(nullptr));
    float daysAgo = (now - memory.timestamp) / 86400.0f;
    float recencyFactor = std::exp(-daysAgo / 7.0f);
    relevance += recencyFactor * 0.2f;
    
    return std::min(relevance, 1.0f);
}

void NPCBrainManager::ConsolidateMemories(int64_t gameTime)
{
    std::vector<NPCBrainMemory> promoted;
    
    for (auto it = m_shortTermMemories.begin(); it != m_shortTermMemories.end(); ) {
        int64_t age = gameTime - it->timestamp;
        int recallCount = it->recallCount;
        
        bool shouldPromote = (recallCount >= 3 && age > 3600) ||
                             (age > 86400 && it->importance > 0.4f) ||
                             (it->importance > 0.7f);
        
        if (shouldPromote) {
            it->memoryType = MemoryType::LongTerm;
            promoted.push_back(*it);
            it = m_shortTermMemories.erase(it);
        } else {
            ++it;
        }
    }
    
    for (const auto& mem : promoted) {
        m_longTermMemories.push_back(mem);
    }
    
    promoted.clear();
    for (auto it = m_longTermMemories.begin(); it != m_longTermMemories.end(); ) {
        if (it->importance > 0.8f && it->recallCount > 10) {
            it->memoryType = MemoryType::Core;
            promoted.push_back(*it);
            it = m_longTermMemories.erase(it);
        } else {
            ++it;
        }
    }
    
    for (const auto& mem : promoted) {
        m_coreMemories.push_back(mem);
    }
}

void NPCBrainManager::PruneMemories()
{
    const size_t MAX_SHORT_TERM = 50;
    const size_t MAX_LONG_TERM = 200;
    
    if (m_shortTermMemories.size() > MAX_SHORT_TERM) {
        std::sort(m_shortTermMemories.begin(), m_shortTermMemories.end(),
                  [](const NPCBrainMemory& a, const NPCBrainMemory& b) {
                      return a.importance > b.importance;
                  });
        m_shortTermMemories.resize(MAX_SHORT_TERM);
    }
    
    if (m_longTermMemories.size() > MAX_LONG_TERM) {
        std::sort(m_longTermMemories.begin(), m_longTermMemories.end(),
                  [](const NPCBrainMemory& a, const NPCBrainMemory& b) {
                      float scoreA = a.importance * 0.7f + 
                                     static_cast<float>(a.recallCount) * 0.03f;
                      float scoreB = b.importance * 0.7f + 
                                     static_cast<float>(b.recallCount) * 0.03f;
                      return scoreA > scoreB;
                  });
        m_longTermMemories.resize(MAX_LONG_TERM);
    }
}

void NPCBrainManager::NightlyConsolidation(int64_t gameTime)
{
    NightlyConsolidationEntry entry;
    entry.timestamp = gameTime;
    entry.shortTermCount = static_cast<int>(m_shortTermMemories.size());
    entry.longTermCount = static_cast<int>(m_longTermMemories.size());
    entry.coreCount = static_cast<int>(m_coreMemories.size());
    
    entry.dreamContent = GenerateDreamSequence();
    
    int emotionsChanged = 0;
    for (auto& emotion : m_emotions) {
        float oldVal = emotion.currentValue;
        emotion.currentValue = emotion.currentValue * 0.7f + emotion.baselineValue * 0.3f;
        if (std::abs(emotion.currentValue - oldVal) > 0.05f) {
            emotionsChanged++;
        }
    }
    entry.emotionsModified = emotionsChanged;
    
    int summarized = 0;
    for (const auto& mem : m_shortTermMemories) {
        if (mem.importance < 0.3f) {
            entry.summarized.push_back(mem.content);
            summarized++;
        }
    }
    entry.summarizedCount = summarized;
    
    m_nightlyConsolidations.push_back(entry);
    if (m_nightlyConsolidations.size() > 30) {
        m_nightlyConsolidations.erase(m_nightlyConsolidations.begin());
    }
    
    std::stringstream dailyLog;
    dailyLog << "Date: " << gameTime << ", "
             << "ShortTermCount: " << entry.shortTermCount << ", "
             << "TraitsModified: " << emotionsChanged << ", "
             << "MemoriesSummarized: " << summarized;
    m_dailySummary.push_back(dailyLog.str());
    if (m_dailySummary.size() > 90) {
        m_dailySummary.erase(m_dailySummary.begin());
    }
}

std::string NPCBrainManager::GenerateDreamSequence() const
{
    std::stringstream dream;
    
    std::vector<NPCBrainMemory> allMemories;
    allMemories.insert(allMemories.end(), m_shortTermMemories.begin(), m_shortTermMemories.end());
    allMemories.insert(allMemories.end(), m_longTermMemories.begin(), m_longTermMemories.end());
    allMemories.insert(allMemories.end(), m_coreMemories.begin(), m_coreMemories.end());
    
    std::sort(allMemories.begin(), allMemories.end(),
              [](const NPCBrainMemory& a, const NPCBrainMemory& b) {
                  return a.importance > b.importance;
              });
    
    dream << "Dream sequence: ";
    int count = 0;
    for (const auto& mem : allMemories) {
        if (count >= 5) break;
        if (!mem.emotion.empty() || mem.importance > 0.5f) {
            if (count > 0) dream << " | ";
            dream << mem.content;
            count++;
        }
    }
    
    if (count == 0) {
        dream << "A peaceful, dreamless sleep.";
    }
    
    return dream.str();
}

void NPCBrainManager::UpdateEmotionDecay(float deltaSeconds)
{
    for (auto& emotion : m_emotions) {
        float decayRate = 0.1f;
        float diff = emotion.baselineValue - emotion.currentValue;
        emotion.currentValue += diff * decayRate * deltaSeconds / 60.0f;
    }
    
    m_emotions.erase(
        std::remove_if(m_emotions.begin(), m_emotions.end(),
                       [](const NPCBrainEmotion& e) {
                           return e.currentValue < 0.01f && 
                                  std::abs(e.currentValue - e.baselineValue) < 0.01f;
                       }),
        m_emotions.end());
}

std::vector<NPCBrainEmotion> NPCBrainManager::DetectEmotions(const std::string& text)
{
    std::vector<NPCBrainEmotion> detected;
    
    std::string textLower = text;
    std::transform(textLower.begin(), textLower.end(), textLower.begin(), ::tolower);
    
    struct EmotionKeyword {
        std::string emotion;
        std::vector<std::string> keywords;
        float defaultIntensity;
    };
    
    std::vector<EmotionKeyword> emotionKeywords = {
        {"joy", {"happy", "joy", "wonderful", "great", "excellent", "love", "beautiful"}, 0.6f},
        {"sadness", {"sad", "unhappy", "depressed", "cry", "tears", "grief", "mourn"}, 0.5f},
        {"anger", {"angry", "furious", "rage", "hate", "despise", "irritated", "mad"}, 0.7f},
        {"fear", {"scared", "afraid", "frightened", "terrified", "panic", "dread", "horror"}, 0.6f},
        {"surprise", {"surprised", "shocked", "amazed", "astonished", "stunned", "unexpected"}, 0.5f},
        {"disgust", {"disgusted", "revolting", "gross", "sickening", "repulsive", "nasty"}, 0.5f},
    };
    
    for (const auto& ek : emotionKeywords) {
        for (const auto& keyword : ek.keywords) {
            if (textLower.find(keyword) != std::string::npos) {
                NPCBrainEmotion emotion;
                emotion.name = ek.emotion;
                emotion.intensity = ek.defaultIntensity;
                emotion.currentValue = ek.defaultIntensity;
                emotion.baselineValue = 0.3f;
                detected.push_back(emotion);
                break;
            }
        }
    }
    
    return detected;
}

void NPCBrainManager::UpdatePersonalityTrait(
    const std::string& traitName, 
    float delta)
{
    for (auto& trait : m_personalityTraits) {
        if (trait.name == traitName) {
            trait.currentValue = std::max(0.0f, std::min(1.0f, 
                                         trait.currentValue + delta));
            return;
        }
    }
    
    PersonalityTrait newTrait;
    newTrait.name = traitName;
    newTrait.currentValue = std::max(0.0f, std::min(1.0f, 0.5f + delta));
    newTrait.baselineValue = 0.5f;
    m_personalityTraits.push_back(newTrait);
}

void NPCBrainManager::AddGoal(const NPCBrainGoal& goal)
{
    m_goalQueue.push_back(goal);
    
    std::sort(m_goalQueue.begin(), m_goalQueue.end(),
              [](const NPCBrainGoal& a, const NPCBrainGoal& b) {
                  return a.priority > b.priority;
              });
}

std::vector<NPCBrainGoal> NPCBrainManager::GetActiveGoals() const
{
    std::vector<NPCBrainGoal> active;
    for (const auto& goal : m_goalQueue) {
        if (goal.progress < goal.targetProgress) {
            active.push_back(goal);
        }
    }
    return active;
}

void NPCBrainManager::UpdateGoalProgress(const std::string& goalId, float progress)
{
    for (auto& goal : m_goalQueue) {
        if (goal.id == goalId) {
            goal.progress = std::min(goal.targetProgress, 
                                     goal.progress + progress);
            return;
        }
    }
}

void NPCBrainManager::RegisterNPC(const std::string& npcId, const std::string& npcName)
{
    for (auto& known : m_knownNPCs) {
        if (known.npcId == npcId) return;
    }
    
    KnownNPC kn;
    kn.npcId = npcId;
    kn.npcName = npcName;
    kn.familiarityLevel = 0.0f;
    kn.relationshipType = "stranger";
    kn.lastInteraction = 0;
    m_knownNPCs.push_back(kn);
}

void NPCBrainManager::UpdateRelationship(
    const std::string& npcId, 
    float familiarityDelta,
    const std::string& interactionNote)
{
    for (auto& known : m_knownNPCs) {
        if (known.npcId == npcId) {
            known.familiarityLevel = std::max(0.0f, std::min(1.0f,
                known.familiarityLevel + familiarityDelta));
            known.lastInteraction = static_cast<int64_t>(time(nullptr));
            
            if (known.familiarityLevel > 0.7f) {
                known.relationshipType = "close_friend";
            } else if (known.familiarityLevel > 0.4f) {
                known.relationshipType = "friend";
            } else if (known.familiarityLevel > 0.2f) {
                known.relationshipType = "acquaintance";
            }
            
            if (!interactionNote.empty()) {
                known.interactionHistory.push_back(interactionNote);
                if (known.interactionHistory.size() > 20) {
                    known.interactionHistory.erase(known.interactionHistory.begin());
                }
            }
            return;
        }
    }
}

KnownNPC NPCBrainManager::GetRelationship(const std::string& npcId) const
{
    for (const auto& known : m_knownNPCs) {
        if (known.npcId == npcId) {
            return known;
        }
    }
    
    KnownNPC unknown;
    unknown.npcId = npcId;
    unknown.familiarityLevel = 0.0f;
    unknown.relationshipType = "unknown";
    return unknown;
}

void NPCBrainManager::AddKnowledge(const std::string& topic, const std::string& content)
{
    m_knowledgeBase[topic] = content;
}

std::string NPCBrainManager::GetKnowledge(const std::string& topic) const
{
    auto it = m_knowledgeBase.find(topic);
    if (it != m_knowledgeBase.end()) {
        return it->second;
    }
    return "";
}

std::string NPCBrainManager::BuildMemoryPrompt() const
{
    std::stringstream prompt;
    
    prompt << "### Core Memories\n";
    for (const auto& mem : m_coreMemories) {
        prompt << "- " << mem.content << "\n";
    }
    
    prompt << "\n### Recent Memories\n";
    auto recent = m_shortTermMemories;
    std::sort(recent.begin(), recent.end(),
              [](const NPCBrainMemory& a, const NPCBrainMemory& b) {
                  return a.timestamp > b.timestamp;
              });
    int count = 0;
    for (const auto& mem : recent) {
        if (count >= 10) break;
        prompt << "- " << mem.content << "\n";
        count++;
    }
    
    return prompt.str();
}

std::string NPCBrainManager::BuildEmotionPrompt() const
{
    std::stringstream prompt;
    prompt << "### Current Emotions\n";
    for (const auto& emotion : m_emotions) {
        prompt << "- " << emotion.name << ": " 
               << std::fixed << std::setprecision(2) << emotion.currentValue << "\n";
    }
    
    prompt << "\n### Personality Traits\n";
    for (const auto& trait : m_personalityTraits) {
        prompt << "- " << trait.name << ": " 
               << std::fixed << std::setprecision(2) << trait.currentValue << "\n";
    }
    
    prompt << "\n### Worldview\n";
    for (const auto& wv : m_worldview) {
        prompt << "- " << wv << "\n";
    }
    
    return prompt.str();
}

std::string NPCBrainManager::BuildFullPrompt() const
{
    std::stringstream prompt;
    
    prompt << BuildEmotionPrompt() << "\n";
    prompt << BuildMemoryPrompt() << "\n";
    
    prompt << "### Active Goals\n";
    auto goals = GetActiveGoals();
    for (const auto& goal : goals) {
        prompt << "- " << goal.description 
               << " (Priority: " << goal.priority 
               << ", Progress: " << goal.progress << "/" << goal.targetProgress << ")\n";
    }
    
    prompt << "\n### Known NPCs\n";
    for (const auto& known : m_knownNPCs) {
        prompt << "- " << known.npcName 
               << " [" << known.relationshipType << "]\n";
    }
    
    return prompt.str();
}

std::string NPCBrainManager::BuildDecisionPrompt() const
{
    std::stringstream prompt;
    
    prompt << "Based on your personality and current state, make a decision.\n\n";
    
    prompt << "Personality:\n";
    for (const auto& trait : m_personalityTraits) {
        prompt << "- " << trait.name << ": " << trait.currentValue << "\n";
    }
    
    prompt << "\nEmotions:\n";
    for (const auto& emotion : m_emotions) {
        prompt << "- " << emotion.name << ": " << emotion.currentValue << "\n";
    }
    
    prompt << "\nGoals:\n";
    auto goals = GetActiveGoals();
    for (const auto& goal : goals) {
        prompt << "- " << goal.description << " (Priority: " << goal.priority << ")\n";
    }
    
    prompt << "\nConsider the following factors:\n";
    prompt << "1. Your personality traits influence your decision-making style\n";
    prompt << "2. Your current emotional state affects your judgment\n";
    prompt << "3. Your active goals should guide your priorities\n";
    prompt << "4. Your relationships with others may impact your choices\n";
    
    return prompt.str();
}

std::vector<std::pair<std::string, float>> 
NPCBrainManager::RankDecisions(const std::vector<std::string>& options) const
{
    std::vector<std::pair<std::string, float>> ranked;
    
    for (const auto& option : options) {
        float score = 0.0f;
        
        for (const auto& goal : m_goalQueue) {
            if (option.find(goal.description) != std::string::npos) {
                score += goal.priority * 0.4f;
            }
        }
        
        for (const auto& emotion : m_emotions) {
            if (emotion.name == "joy" && option.find("happy") != std::string::npos) {
                score += emotion.currentValue * 0.2f;
            }
            if (emotion.name == "fear" && option.find("safe") != std::string::npos) {
                score += emotion.currentValue * 0.2f;
            }
        }
        
        float openness = 0.5f;
        float conscientiousness = 0.5f;
        for (const auto& trait : m_personalityTraits) {
            if (trait.name == "openness") openness = trait.currentValue;
            if (trait.name == "conscientiousness") conscientiousness = trait.currentValue;
        }
        
        if (option.find("explore") != std::string::npos ||
            option.find("new") != std::string::npos) {
            score += openness * 0.15f;
        }
        if (option.find("plan") != std::string::npos ||
            option.find("organize") != std::string::npos) {
            score += conscientiousness * 0.15f;
        }
        
        ranked.push_back({option, score});
    }
    
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });
    
    return ranked;
}

void NPCBrainManager::ExtractKnowledgeFromUtterance(const std::string& utterance)
{
    std::string text = utterance;
    
    std::string textLower = text;
    std::transform(textLower.begin(), textLower.end(), textLower.begin(), ::tolower);
    
    if (textLower.find("i know") != std::string::npos ||
        textLower.find("i learned") != std::string::npos ||
        textLower.find("did you know") != std::string::npos) {
        
        NPCBrainMemory mem;
        mem.content = text;
        mem.emotion = "curiosity";
        mem.importance = 0.4f;
        mem.memoryType = MemoryType::ShortTerm;
        AddMemory(mem);
    }
    
    std::vector<std::string> topics = {
        "weather", "politics", "religion", "science", "history", 
        "art", "music", "food", "travel", "technology"
    };
    
    for (const auto& topic : topics) {
        if (textLower.find(topic) != std::string::npos) {
            AddKnowledge(topic, text);
        }
    }
}

void NPCBrainManager::AbsorbOverheardKnowledge(
    const std::string& speakerId,
    const std::string& utterance)
{
    float familiarity = 0.0f;
    for (const auto& known : m_knownNPCs) {
        if (known.npcId == speakerId) {
            familiarity = known.familiarityLevel;
            break;
        }
    }
    
    float absorbChance = 0.1f + familiarity * 0.3f;
    
    float randVal = static_cast<float>(rand()) / RAND_MAX;
    if (randVal < absorbChance) {
        NPCBrainMemory mem;
        mem.content = "Overheard: " + utterance;
        mem.importance = 0.3f + familiarity * 0.2f;
        mem.memoryType = MemoryType::ShortTerm;
        mem.emotion = "neutral";
        AddMemory(mem);
    }
}

void NPCBrainManager::InteractWith(
    const std::string& npcId,
    const std::string& interactionType,
    const std::string& description)
{
    m_interactionCounts[npcId]++;
    
    float delta = 0.05f;
    if (interactionType == "positive") {
        delta = 0.1f;
    } else if (interactionType == "negative") {
        delta = -0.1f;
    }
    
    UpdateRelationship(npcId, delta, description);
}

json NPCBrainManager::SerializeToJSON() const
{
    json j;
    
    j["config"]["npcType"] = static_cast<int>(m_config.npcType);
    
    json traitsArr = json::array();
    for (const auto& trait : m_personalityTraits) {
        json tj;
        tj["name"] = trait.name;
        tj["currentValue"] = trait.currentValue;
        tj["baselineValue"] = trait.baselineValue;
        traitsArr.push_back(tj);
    }
    j["personalityTraits"] = traitsArr;
    
    json emotionsArr = json::array();
    for (const auto& emotion : m_emotions) {
        json ej;
        ej["name"] = emotion.name;
        ej["intensity"] = emotion.intensity;
        ej["currentValue"] = emotion.currentValue;
        ej["baselineValue"] = emotion.baselineValue;
        emotionsArr.push_back(ej);
    }
    j["emotions"] = emotionsArr;
    
    j["worldview"] = m_worldview;
    
    auto serializeMemories = [](const std::vector<NPCBrainMemory>& memories) {
        json arr = json::array();
        for (const auto& mem : memories) {
            json mj;
            mj["id"] = mem.id;
            mj["content"] = mem.content;
            mj["emotion"] = mem.emotion;
            mj["importance"] = mem.importance;
            mj["timestamp"] = mem.timestamp;
            mj["recallCount"] = mem.recallCount;
            mj["memoryType"] = static_cast<int>(mem.memoryType);
            arr.push_back(mj);
        }
        return arr;
    };
    
    j["shortTermMemories"] = serializeMemories(m_shortTermMemories);
    j["longTermMemories"] = serializeMemories(m_longTermMemories);
    j["coreMemories"] = serializeMemories(m_coreMemories);
    
    json goalsArr = json::array();
    for (const auto& goal : m_goalQueue) {
        json gj;
        gj["id"] = goal.id;
        gj["description"] = goal.description;
        gj["priority"] = goal.priority;
        gj["progress"] = goal.progress;
        gj["targetProgress"] = goal.targetProgress;
        goalsArr.push_back(gj);
    }
    j["goals"] = goalsArr;
    
    json npcsArr = json::array();
    for (const auto& known : m_knownNPCs) {
        json kj;
        kj["npcId"] = known.npcId;
        kj["npcName"] = known.npcName;
        kj["familiarityLevel"] = known.familiarityLevel;
        kj["relationshipType"] = known.relationshipType;
        kj["lastInteraction"] = known.lastInteraction;
        kj["interactionHistory"] = known.interactionHistory;
        npcsArr.push_back(kj);
    }
    j["knownNPCs"] = npcsArr;
    
    json kbObj = json::object();
    for (const auto& kv : m_knowledgeBase) {
        kbObj[kv.first] = kv.second;
    }
    j["knowledgeBase"] = kbObj;
    
    j["nextMemoryId"] = m_nextMemoryId;
    
    json moodArr = json::array();
    for (const auto& entry : m_moodHistory) {
        json mej;
        mej["emotionName"] = entry.emotionName;
        mej["intensity"] = entry.intensity;
        mej["timestamp"] = entry.timestamp;
        mej["trigger"] = entry.trigger;
        moodArr.push_back(mej);
    }
    j["moodHistory"] = moodArr;
    
    j["dailySummary"] = m_dailySummary;
    
    json nightlyArr = json::array();
    for (const auto& nc : m_nightlyConsolidations) {
        json ncj;
        ncj["timestamp"] = nc.timestamp;
        ncj["shortTermCount"] = nc.shortTermCount;
        ncj["longTermCount"] = nc.longTermCount;
        ncj["coreCount"] = nc.coreCount;
        ncj["dreamContent"] = nc.dreamContent;
        ncj["summarizedCount"] = nc.summarizedCount;
        ncj["emotionsModified"] = nc.emotionsModified;
        ncj["summarized"] = nc.summarized;
        nightlyArr.push_back(ncj);
    }
    j["nightlyConsolidations"] = nightlyArr;
    
    j["lastConsolidationTime"] = m_lastConsolidationTime;
    
    return j;
}

void NPCBrainManager::DeserializeFromJSON(const json& j)
{
    if (j.contains("config") && j["config"].contains("npcType")) {
        m_config.npcType = static_cast<NPCType>(j["config"]["npcType"].get<int>());
    }
    
    m_personalityTraits.clear();
    if (j.contains("personalityTraits")) {
        for (const auto& tj : j["personalityTraits"]) {
            PersonalityTrait trait;
            trait.name = tj["name"].get<std::string>();
            trait.currentValue = tj["currentValue"].get<float>();
            trait.baselineValue = tj.value("baselineValue", trait.currentValue);
            m_personalityTraits.push_back(trait);
        }
    }
    
    m_emotions.clear();
    if (j.contains("emotions")) {
        for (const auto& ej : j["emotions"]) {
            NPCBrainEmotion emotion;
            emotion.name = ej["name"].get<std::string>();
            emotion.intensity = ej["intensity"].get<float>();
            emotion.currentValue = ej.value("currentValue", emotion.intensity);
            emotion.baselineValue = ej.value("baselineValue", 0.5f);
            m_emotions.push_back(emotion);
        }
    }
    
    m_worldview.clear();
    if (j.contains("worldview")) {
        for (const auto& wv : j["worldview"]) {
            m_worldview.push_back(wv.get<std::string>());
        }
    }
    
    auto deserializeMemories = [](const json& arr) {
        std::vector<NPCBrainMemory> memories;
        for (const auto& mj : arr) {
            NPCBrainMemory mem;
            mem.id = mj["id"].get<int>();
            mem.content = mj["content"].get<std::string>();
            mem.emotion = mj.value("emotion", "");
            mem.importance = mj["importance"].get<float>();
            mem.timestamp = mj["timestamp"].get<int64_t>();
            mem.recallCount = mj.value("recallCount", 0);
            mem.memoryType = static_cast<MemoryType>(
                mj.value("memoryType", 0));
            memories.push_back(mem);
        }
        return memories;
    };
    
    m_shortTermMemories = deserializeMemories(j.value("shortTermMemories", json::array()));
    m_longTermMemories = deserializeMemories(j.value("longTermMemories", json::array()));
    m_coreMemories = deserializeMemories(j.value("coreMemories", json::array()));
    
    m_goalQueue.clear();
    if (j.contains("goals")) {
        for (const auto& gj : j["goals"]) {
            NPCBrainGoal goal;
            goal.id = gj["id"].get<std::string>();
            goal.description = gj["description"].get<std::string>();
            goal.priority = gj["priority"].get<float>();
            goal.progress = gj["progress"].get<float>();
            goal.targetProgress = gj["targetProgress"].get<float>();
            m_goalQueue.push_back(goal);
        }
    }
    
    m_knownNPCs.clear();
    if (j.contains("knownNPCs")) {
        for (const auto& kj : j["knownNPCs"]) {
            KnownNPC known;
            known.npcId = kj["npcId"].get<std::string>();
            known.npcName = kj["npcName"].get<std::string>();
            known.familiarityLevel = kj["familiarityLevel"].get<float>();
            known.relationshipType = kj["relationshipType"].get<std::string>();
            known.lastInteraction = kj.value("lastInteraction", 0);
            if (kj.contains("interactionHistory")) {
                for (const auto& ih : kj["interactionHistory"]) {
                    known.interactionHistory.push_back(ih.get<std::string>());
                }
            }
            m_knownNPCs.push_back(known);
        }
    }
    
    m_knowledgeBase.clear();
    if (j.contains("knowledgeBase")) {
        for (auto it = j["knowledgeBase"].begin(); 
             it != j["knowledgeBase"].end(); ++it) {
            m_knowledgeBase[it.key()] = it.value().get<std::string>();
        }
    }
    
    m_nextMemoryId = j.value("nextMemoryId", 0);
    
    m_moodHistory.clear();
    if (j.contains("moodHistory")) {
        for (const auto& mej : j["moodHistory"]) {
            MoodHistoryEntry entry;
            entry.emotionName = mej["emotionName"].get<std::string>();
            entry.intensity = mej["intensity"].get<float>();
            entry.timestamp = mej["timestamp"].get<int64_t>();
            entry.trigger = mej.value("trigger", "");
            m_moodHistory.push_back(entry);
        }
    }
    
    m_dailySummary.clear();
    if (j.contains("dailySummary")) {
        for (const auto& ds : j["dailySummary"]) {
            m_dailySummary.push_back(ds.get<std::string>());
        }
    }
    
    m_nightlyConsolidations.clear();
    if (j.contains("nightlyConsolidations")) {
        for (const auto& ncj : j["nightlyConsolidations"]) {
            NightlyConsolidationEntry nc;
            nc.timestamp = ncj["timestamp"].get<int64_t>();
            nc.shortTermCount = ncj["shortTermCount"].get<int>();
            nc.longTermCount = ncj["longTermCount"].get<int>();
            nc.coreCount = ncj["coreCount"].get<int>();
            nc.dreamContent = ncj["dreamContent"].get<std::string>();
            nc.summarizedCount = ncj["summarizedCount"].get<int>();
            nc.emotionsModified = ncj["emotionsModified"].get<int>();
            if (ncj.contains("summarized")) {
                for (const auto& s : ncj["summarized"]) {
                    nc.summarized.push_back(s.get<std::string>());
                }
            }
            m_nightlyConsolidations.push_back(nc);
        }
    }
    
    m_lastConsolidationTime = j.value("lastConsolidationTime", 0);
}

} // namespace npc