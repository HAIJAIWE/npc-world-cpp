#pragma once
#include "npc_brain_types.h"
#include <string>
#include <vector>
#include <cstdint>

class NPCBrainManager;

struct IntegrityReport {
    std::string npc_id;
    std::string npc_name;
    bool healthy = true;
    std::vector<std::string> issues;
    std::vector<std::string> fixes;
    struct Stats {
        int total_memories = 0;
        int short_term = 0;
        int long_term = 0;
        int core = 0;
        int knowledge = 0;
        int reflections = 0;
        int relationships = 0;
        int ruminations = 0;
        int training_records = 0;
    } stats;
};

class NPCDataGuard {
public:
    explicit NPCDataGuard(NPCBrainManager* brain_manager);
    ~NPCDataGuard() = default;

    IntegrityReport check_integrity(const std::string& npc_id);

    std::vector<std::string> repair_data(const IntegrityReport& report);

    std::vector<IntegrityReport> scan_all_npcs();

private:
    NPCBrainManager* brain_mgr_ = nullptr;

    std::string get_npc_name(const std::string& npc_id);
};
