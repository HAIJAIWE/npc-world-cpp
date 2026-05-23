#include "../../include/behavior/npc_emotion.h"
#include "../../include/common/npc_types.h"
#include <algorithm>
#include <cmath>

namespace npc {

NPCEmotionService& NPCEmotionService::Instance()
{
    static NPCEmotionService instance;
    return instance;
}

NPCEmotionService::NPCEmotionService()
{
}

NPCEmotionService::~NPCEmotionService()
{
}

void NPCEmotionService::Initialize()
{
    m_contexts.clear();
    m_reciprocityLedger.clear();
}

void NPCEmotionService::CreateEmotionContext(const std::string& npcId)
{
    if (m_contexts.find(npcId) != m_contexts.end()) {
        return;
    }

    NPCEmotionalContext ctx;
    ctx.npcId = npcId;

    ctx.currentEmotions["joy"] = NPCEmotionEntry();
    ctx.currentEmotions["joy"].name = "joy";
    ctx.currentEmotions["joy"].intensity = 0.3f;
    ctx.currentEmotions["joy"].valence = 0.7f;
    ctx.currentEmotions["joy"].decayRate = 0.05f;

    ctx.currentEmotions["sadness"] = NPCEmotionEntry();
    ctx.currentEmotions["sadness"].name = "sadness";
    ctx.currentEmotions["sadness"].intensity = 0.1f;
    ctx.currentEmotions["sadness"].valence = -0.5f;
    ctx.currentEmotions["sadness"].decayRate = 0.08f;

    ctx.currentEmotions["anger"] = NPCEmotionEntry();
    ctx.currentEmotions["anger"].name = "anger";
    ctx.currentEmotions["anger"].intensity = 0.1f;
    ctx.currentEmotions["anger"].valence = -0.6f;
    ctx.currentEmotions["anger"].decayRate = 0.1f;

    ctx.currentEmotions["fear"] = NPCEmotionEntry();
    ctx.currentEmotions["fear"].name = "fear";
    ctx.currentEmotions["fear"].intensity = 0.1f;
    ctx.currentEmotions["fear"].valence = -0.7f;
    ctx.currentEmotions["fear"].decayRate = 0.07f;

    ctx.currentEmotions["neutral"] = NPCEmotionEntry();
    ctx.currentEmotions["neutral"].name = "neutral";
    ctx.currentEmotions["neutral"].intensity = 0.5f;
    ctx.currentEmotions["neutral"].valence = 0.0f;
    ctx.currentEmotions["neutral"].decayRate = 0.03f;

    ctx.mentalEnergy = 1.0f;
    ctx.maxMentalEnergy = 1.0f;
    ctx.mentalEnergyRecoveryRate = 0.1f;
    ctx.system2FatigueMultiplier = 2.0f;
    ctx.system1Active = false;

    ctx.regulationStyle = EmotionRegulationStyle::Reappraisal;
    ctx.ruminationTendency = 0.3f;
    ctx.emotionalStability = 0.5f;
    ctx.bioRhythm = 0.5f;
    ctx.bioRhythmAmplitude = 0.3f;
    ctx.bioRhythmPeriod = 86400.0f;

    m_contexts[npcId] = ctx;
}

void NPCEmotionService::RemoveEmotionContext(const std::string& npcId)
{
    m_contexts.erase(npcId);

    auto ledgerIt = m_reciprocityLedger.find(npcId);
    if (ledgerIt != m_reciprocityLedger.end()) {
        m_reciprocityLedger.erase(ledgerIt);
    }

    for (auto& pair : m_reciprocityLedger) {
        pair.second.erase(npcId);
    }
}

void NPCEmotionService::UpdateMentalEnergy(
    const std::string& npcId, float deltaSeconds)
{
    auto it = m_contexts.find(npcId);
    if (it == m_contexts.end()) return;

    auto& ctx = it->second;

    float recoveryAmount = ctx.mentalEnergyRecoveryRate * deltaSeconds;

    if (!ctx.system1Active && ctx.mentalEnergy < 0.3f) {
        recoveryAmount *= 1.5f;
    }

    float fatigueMultiplier = 1.0f;
    if (ctx.system2FatigueMultiplier > 1.0f) {
        fatigueMultiplier = 1.0f / ctx.system2FatigueMultiplier;
    }
    recoveryAmount *= fatigueMultiplier;

    float bioFactor = CalculateBioRhythm(npcId, 0);
    recoveryAmount *= (0.8f + 0.4f * bioFactor);

    ctx.mentalEnergy = std::min(ctx.maxMentalEnergy,
                                 ctx.mentalEnergy + recoveryAmount);

    if (!ctx.system1Active) {
        float system1Threshold = 0.15f + ctx.emotionalStability * 0.1f;
        if (ctx.mentalEnergy > system1Threshold) {
            ctx.system1Active = true;
        }
    }

    float system1DropThreshold = 0.05f;
    if (ctx.system1Active && ctx.mentalEnergy < system1DropThreshold) {
        ctx.system1Active = false;
    }
}

void NPCEmotionService::ConsumeMentalEnergy(
    const std::string& npcId, float amount)
{
    auto it = m_contexts.find(npcId);
    if (it == m_contexts.end()) return;

    it->second.mentalEnergy = std::max(0.0f,
                                        it->second.mentalEnergy - amount);

    if (it->second.mentalEnergy < 0.1f) {
        it->second.system2FatigueMultiplier = std::min(5.0f,
            it->second.system2FatigueMultiplier + 0.5f);
    }
}

void NPCEmotionService::UpdateEmotionState(
    const std::string& npcId, float deltaSeconds)
{
    auto it = m_contexts.find(npcId);
    if (it == m_contexts.end()) return;

    auto& ctx = it->second;

    for (auto& pair : ctx.currentEmotions) {
        auto& emotion = pair.second;

        float decayAmount = emotion.decayRate * deltaSeconds;

        switch (ctx.regulationStyle) {
            case EmotionRegulationStyle::Suppression:
                decayAmount *= 0.5f;
                break;
            case EmotionRegulationStyle::Reappraisal:
                if (emotion.valence < 0) {
                    decayAmount *= 1.5f;
                }
                break;
            case EmotionRegulationStyle::Distraction:
                decayAmount *= 1.0f;
                break;
            case EmotionRegulationStyle::Expression:
                decayAmount *= 2.0f;
                break;
        }

        decayAmount *= (1.0f - ctx.ruminationTendency * 0.5f);

        emotion.intensity = std::max(0.0f, emotion.intensity - decayAmount);

        if (emotion.intensity < 0.01f) {
            emotion.intensity = 0.0f;
        }
    }

    UpdateBioRhythm(npcId, deltaSeconds);
    UpdateSelfDiscrepancy(npcId);
}

NPCEmotionEntry NPCEmotionService::GetDominantEmotion(
    const std::string& npcId) const
{
    auto it = m_contexts.find(npcId);
    if (it == m_contexts.end()) {
        NPCEmotionEntry neutral;
        neutral.name = "neutral";
        neutral.intensity = 0.5f;
        neutral.valence = 0.0f;
        return neutral;
    }

    const auto& emotions = it->second.currentEmotions;
    const NPCEmotionEntry* dominant = nullptr;
    float maxIntensity = -1.0f;

    for (const auto& pair : emotions) {
        if (pair.second.intensity > maxIntensity) {
            maxIntensity = pair.second.intensity;
            dominant = &pair.second;
        }
    }

    if (dominant) return *dominant;

    NPCEmotionEntry neutral;
    neutral.name = "neutral";
    neutral.intensity = 0.5f;
    neutral.valence = 0.0f;
    return neutral;
}

float NPCEmotionService::GetEmotionIntensity(
    const std::string& npcId, const std::string& emotionName) const
{
    auto it = m_contexts.find(npcId);
    if (it == m_contexts.end()) return 0.0f;

    auto emotionIt = it->second.currentEmotions.find(emotionName);
    if (emotionIt != it->second.currentEmotions.end()) {
        return emotionIt->second.intensity;
    }
    return 0.0f;
}

void NPCEmotionService::TriggerEmotion(
    const std::string& npcId,
    const std::string& emotionName,
    float intensity,
    const std::string& trigger)
{
    auto it = m_contexts.find(npcId);
    if (it == m_contexts.end()) return;

    auto& ctx = it->second;

    float adjustedIntensity = intensity * (1.0f + ctx.emotionalStability * 0.3f);

    ctx.currentEmotions[emotionName].name = emotionName;
    ctx.currentEmotions[emotionName].intensity =
        std::min(1.0f, ctx.currentEmotions[emotionName].intensity + adjustedIntensity);

    if (!trigger.empty()) {
        EmotionEvent event;
        event.emotionName = emotionName;
        event.intensity = adjustedIntensity;
        event.timestamp = 0;
        event.trigger = trigger;
        ctx.emotionHistory.push_back(event);

        if (ctx.emotionHistory.size() > 100) {
            ctx.emotionHistory.erase(ctx.emotionHistory.begin());
        }
    }
}

void NPCEmotionService::UpdateAfterInteraction(
    const std::string& npcIdA,
    const std::string& npcIdB,
    float sentimentScore)
{
    auto itA = m_contexts.find(npcIdA);
    auto itB = m_contexts.find(npcIdB);

    if (itA != m_contexts.end()) {
        auto& ctxA = itA->second;
        ctxA.interactionPartners[npcIdB]++;

        if (sentimentScore > 0.5f) {
            TriggerEmotion(npcIdA, "joy", 0.2f, "Positive interaction");
        } else if (sentimentScore < -0.5f) {
            TriggerEmotion(npcIdA, "anger", 0.2f, "Negative interaction");
        }

        float energyCost = 0.05f + std::abs(sentimentScore) * 0.1f;
        ConsumeMentalEnergy(npcIdA, energyCost);
    }

    if (itB != m_contexts.end()) {
        auto& ctxB = itB->second;
        ctxB.interactionPartners[npcIdA]++;

        if (sentimentScore > 0.5f) {
            TriggerEmotion(npcIdB, "joy", 0.2f, "Positive interaction");
        } else if (sentimentScore < -0.5f) {
            TriggerEmotion(npcIdB, "sadness", 0.2f, "Negative interaction");
        }

        float energyCost = 0.05f + std::abs(sentimentScore) * 0.1f;
        ConsumeMentalEnergy(npcIdB, energyCost);
    }
}

void NPCEmotionService::UpdateBioRhythm(
    const std::string& npcId, float deltaSeconds)
{
    auto it = m_contexts.find(npcId);
    if (it == m_contexts.end()) return;

    auto& ctx = it->second;

    float newRhythm = CalculateBioRhythm(npcId, 0);
    ctx.bioRhythm = newRhythm;

    for (auto& pair : ctx.currentEmotions) {
        float bioFactor = 1.0f + (newRhythm - 0.5f) * ctx.bioRhythmAmplitude;
        bioFactor = std::max(0.5f, std::min(1.5f, bioFactor));
        pair.second.intensity *= bioFactor;
        pair.second.intensity = std::min(1.0f, pair.second.intensity);
    }
}

float NPCEmotionService::CalculateBioRhythm(
    const std::string& npcId, int64_t gameTime) const
{
    auto it = m_contexts.find(npcId);
    if (it == m_contexts.end()) return 0.5f;

    const auto& ctx = it->second;
    float period = ctx.bioRhythmPeriod;
    float phase = static_cast<float>(gameTime % static_cast<int64_t>(period)) / period;
    float rhythm = static_cast<float>(std::sin(phase * 2.0 * M_PI));
    return (rhythm + 1.0f) * 0.5f;
}

void NPCEmotionService::GeneratePostInteractionRumination(
    const std::string& npcId,
    const std::string& targetId,
    float sentimentScore)
{
    auto it = m_contexts.find(npcId);
    if (it == m_contexts.end()) return;

    auto& ctx = it->second;

    if (std::abs(sentimentScore) > 0.3f && ctx.ruminationTendency > 0.5f) {
        float ruminationAmount = std::abs(sentimentScore) * ctx.ruminationTendency * 0.1f;

        if (sentimentScore > 0) {
            TriggerEmotion(npcId, "joy", ruminationAmount, "Positive rumination");
        } else {
            TriggerEmotion(npcId, "sadness", ruminationAmount, "Negative rumination");
        }
    }
}

void NPCEmotionService::UpdateSelfDiscrepancy(const std::string& npcId)
{
    auto it = m_contexts.find(npcId);
    if (it == m_contexts.end()) return;

    auto& ctx = it->second;

    float idealGap = 0.0f;
    float actualJoy = ctx.currentEmotions["joy"].intensity;
    float idealJoy = 0.7f;
    idealGap = std::abs(actualJoy - idealJoy);

    float oughtGap = 0.0f;
    float actualAnger = ctx.currentEmotions["anger"].intensity;
    float oughtAnger = 0.1f;
    oughtGap = std::abs(actualAnger - oughtAnger);

    ctx.selfDiscrepancies["ideal"] = idealGap;
    ctx.selfDiscrepancies["ought"] = oughtGap;

    if (idealGap > 0.3f) {
        TriggerEmotion(npcId, "sadness", 0.1f, "Self-discrepancy");
    }
    if (oughtGap > 0.3f) {
        TriggerEmotion(npcId, "fear", 0.1f, "Self-discrepancy");
    }
}

void NPCEmotionService::ProcessReciprocity(
    const std::string& npcId,
    const std::string& targetId,
    float helpAmount)
{
    if (helpAmount > 0) {
        m_reciprocityLedger[npcId][targetId] += helpAmount;
    } else {
        auto it = m_reciprocityLedger.find(targetId);
        if (it != m_reciprocityLedger.end()) {
            auto targetIt = it->second.find(npcId);
            if (targetIt != it->second.end()) {
                float owed = targetIt->second;
                if (owed > 0) {
                    float repaid = std::min(owed, std::abs(helpAmount));
                    targetIt->second -= repaid;

                    TriggerEmotion(npcId, "joy", 0.15f, "Reciprocity satisfied");
                }
            }
        }
    }
}

float NPCEmotionService::GetReciprocityBalance(
    const std::string& npcId, const std::string& targetId) const
{
    auto it = m_reciprocityLedger.find(npcId);
    if (it == m_reciprocityLedger.end()) return 0.0f;

    auto targetIt = it->second.find(targetId);
    if (targetIt == it->second.end()) return 0.0f;

    return targetIt->second;
}

NPCEmotionalContext NPCEmotionService::GetEmotionalContext(
    const std::string& npcId) const
{
    auto it = m_contexts.find(npcId);
    if (it != m_contexts.end()) {
        return it->second;
    }

    NPCEmotionalContext empty;
    empty.npcId = npcId;
    return empty;
}

void NPCEmotionService::SetEmotionRegulationStyle(
    const std::string& npcId, EmotionRegulationStyle style)
{
    auto it = m_contexts.find(npcId);
    if (it != m_contexts.end()) {
        it->second.regulationStyle = style;
    }
}

void NPCEmotionService::SetEmotionalStability(
    const std::string& npcId, float stability)
{
    auto it = m_contexts.find(npcId);
    if (it != m_contexts.end()) {
        it->second.emotionalStability = std::max(0.0f, std::min(1.0f, stability));
    }
}

void NPCEmotionService::SetRuminationTendency(
    const std::string& npcId, float tendency)
{
    auto it = m_contexts.find(npcId);
    if (it != m_contexts.end()) {
        it->second.ruminationTendency = std::max(0.0f, std::min(1.0f, tendency));
    }
}

} // namespace npc