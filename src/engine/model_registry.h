#pragma once

#include "engine/model_engine.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace npc {

struct NpcModelProfile {
    std::string npc_id;
    std::string model_path;
    std::string lora_path;
    ModelParams sampling_params;
    bool is_remote = false;
    std::string remote_provider;
    bool enabled = true;
};

struct NpcAssignResult {
    bool success;
    bool model_switched;
    std::string error;
};

class ModelRegistry {
public:
    static ModelRegistry& instance();

    NpcAssignResult assignNpc(const std::string& npcId,
                               const std::string& modelPath,
                               const std::string& loraPath = "",
                               const ModelParams* params = nullptr);

    NpcAssignResult assignNpcRemote(const std::string& npcId,
                                     const std::string& provider,
                                     const ModelParams* params = nullptr);

    bool unassignNpc(const std::string& npcId);

    const NpcModelProfile* getProfile(const std::string& npcId) const;

    std::vector<std::string> getNpcsForModel(const std::string& modelPath) const;

    ModelParams getParamsForNpc(const std::string& npcId) const;

    bool ensureModelForNpc(const std::string& npcId);

    std::string currentModelPath() const;

    bool preloadForNpc(const std::string& npcId);

    size_t profileCount() const;

    std::vector<NpcModelProfile> allProfiles() const;

private:
    ModelRegistry() = default;
    ModelRegistry(const ModelRegistry&) = delete;
    ModelRegistry& operator=(const ModelRegistry&) = delete;

    std::string m_currentModelPath;
    std::unordered_map<std::string, NpcModelProfile> m_profiles;
    mutable std::mutex m_mutex;
};

} // namespace npc
