#include "engine/model_registry.h"
#include <cstdio>

namespace npc {

ModelRegistry& ModelRegistry::instance() {
    static ModelRegistry registry;
    return registry;
}

NpcAssignResult ModelRegistry::assignNpc(const std::string& npcId,
                                          const std::string& modelPath,
                                          const std::string& loraPath,
                                          const ModelParams* params) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (modelPath.empty()) {
        return {false, false, "model path is empty"};
    }

    bool pathChanged = false;
    auto it = m_profiles.find(npcId);
    if (it != m_profiles.end() && it->second.model_path != modelPath) {
        pathChanged = true;
    }

    NpcModelProfile profile;
    profile.npc_id = npcId;
    profile.model_path = modelPath;
    profile.lora_path = loraPath;
    if (params) {
        profile.sampling_params = *params;
    } else {
        profile.sampling_params = ModelParams{};
    }
    m_profiles[npcId] = profile;

    fprintf(stdout, "[ModelRegistry] NPC '%s' -> model '%s' (lora=%s)\n",
        npcId.c_str(), modelPath.c_str(),
        loraPath.empty() ? "none" : loraPath.c_str());

    bool modelSwitched = false;
    if (m_currentModelPath.empty() || pathChanged) {
        if (ensureModelForNpc(npcId)) {
            modelSwitched = true;
        } else if (pathChanged) {
            fprintf(stderr, "[ModelRegistry] Failed to switch model for '%s'\n", npcId.c_str());
        }
    }

    return {true, modelSwitched, ""};
}

NpcAssignResult ModelRegistry::assignNpcRemote(const std::string& npcId,
                                                const std::string& provider,
                                                const ModelParams* params) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (provider.empty()) {
        return {false, false, "provider name is empty"};
    }

    NpcModelProfile profile;
    profile.npc_id = npcId;
    profile.model_path = "remote://" + provider;
    profile.is_remote = true;
    profile.remote_provider = provider;
    if (params) {
        profile.sampling_params = *params;
    } else {
        profile.sampling_params = ModelParams{};
    }
    m_profiles[npcId] = profile;

    fprintf(stdout, "[ModelRegistry] NPC '%s' -> remote provider '%s'\n",
        npcId.c_str(), provider.c_str());

    return {true, false, ""};
}

bool ModelRegistry::unassignNpc(const std::string& npcId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profiles.erase(npcId) > 0;
}

const NpcModelProfile* ModelRegistry::getProfile(const std::string& npcId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_profiles.find(npcId);
    return it != m_profiles.end() ? &it->second : nullptr;
}

std::vector<std::string> ModelRegistry::getNpcsForModel(const std::string& modelPath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> result;
    for (const auto& [id, profile] : m_profiles) {
        if (profile.model_path == modelPath) result.push_back(id);
    }
    return result;
}

ModelParams ModelRegistry::getParamsForNpc(const std::string& npcId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_profiles.find(npcId);
    if (it != m_profiles.end()) return it->second.sampling_params;
    return ModelEngine::instance().getDefaultParams();
}

bool ModelRegistry::ensureModelForNpc(const std::string& npcId) {
    auto* profile = getProfile(npcId);
    if (!profile) return false;

    auto& engine = ModelEngine::instance();

    if (m_currentModelPath == profile->model_path && engine.isModelLoaded()) {
        engine.setDefaultParams(profile->sampling_params);
        return true;
    }

    if (engine.isModelLoaded()) {
        fprintf(stdout, "[ModelRegistry] Switching model: %s \u2192 %s\n",
            m_currentModelPath.c_str(), profile->model_path.c_str());
        engine.unloadModel();
    }

    engine.setDefaultParams(profile->sampling_params);

    if (!engine.loadModel(profile->model_path)) {
        fprintf(stderr, "[ModelRegistry] Failed to load model: %s\n", profile->model_path.c_str());
        return false;
    }

    m_currentModelPath = profile->model_path;
    fprintf(stdout, "[ModelRegistry] Model loaded for NPC '%s': %s\n",
        npcId.c_str(), profile->model_path.c_str());
    return true;
}

std::string ModelRegistry::currentModelPath() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentModelPath;
}

bool ModelRegistry::preloadForNpc(const std::string& npcId) {
    return ensureModelForNpc(npcId);
}

size_t ModelRegistry::profileCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profiles.size();
}

std::vector<NpcModelProfile> ModelRegistry::allProfiles() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<NpcModelProfile> result;
    result.reserve(m_profiles.size());
    for (const auto& [id, profile] : m_profiles) result.push_back(profile);
    return result;
}

} // namespace npc
