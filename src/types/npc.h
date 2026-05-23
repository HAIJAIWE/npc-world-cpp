#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace npc {

struct PersonalityTraits {
    std::string speaking_style;
    std::string tone;
    std::string humor;
    std::string emotional_range;
};

struct BackgroundInfo {
    std::string history;
    std::string secrets;
    std::string occupation;
    std::string origin;
};

struct KnowledgeDomain {
    std::string domains;
    std::string ignorance;
};

struct BehaviorPattern {
    std::string daily_routine;
    std::string likes;
    std::string dislikes;
    std::string habits;
};

struct NPCEntity {
    std::string  id;
    std::string  name;
    std::string  aliases;
    int32_t      age       = 0;
    std::string  gender;
    std::string  role;
    std::string  personality;
    std::string  background;
    std::string  knowledge;
    std::string  behavior;
    int64_t      created_at = 0;
    int64_t      updated_at = 0;
};

struct NPCRow {
    std::string  id;
    std::string  name;
    std::string  aliases;
    std::optional<int32_t>  age;
    std::optional<std::string> gender;
    std::optional<std::string> role;
    std::string  personality;
    std::string  background;
    std::string  knowledge;
    std::string  behavior;
    int64_t      created_at = 0;
    int64_t      updated_at = 0;
};

} // namespace npc
