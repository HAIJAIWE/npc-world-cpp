#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <memory>
#include <nlohmann/json.hpp>

struct WorldFlag {
    std::string key;
    std::string value;
    std::string severity = "significant";
    int         chapter_set = 0;
};

struct NpcWorldState {
    std::string                    npc_id;
    std::string                    npc_name;
    std::string                    current_location_id;
    int                            current_chapter = 1;
    std::vector<WorldFlag>         active_flags;
    int64_t                        last_tick_timestamp = 0;
};

struct RelationWorldState {
    std::string npc_a;
    std::string npc_b;
    float       affection_trend            = 0.5f;
    int64_t     last_interaction_timestamp = 0;
    float       tension_level              = 0.0f;
};

struct EventLogEntry {
    std::string            id;
    int64_t                timestamp = 0;
    std::string            type;
    std::string            speaker;
    std::string            listener;
    std::string            content;
    std::string            emotion         = "neutral";
    float                  emotion_intensity = 0;
    std::string            location_id;
    std::string            location_name;
    int                    chapter         = 0;
    int                    scene_index     = 0;
    std::vector<WorldFlag> world_flags;
    std::vector<std::string> affected_npcs;
    float                  narrative_weight = 0.5f;
};

struct SceneSummary {
    int                    scene_index    = 0;
    int                    chapter        = 0;
    std::string            location_id;
    int64_t                start_timestamp = 0;
    int64_t                end_timestamp   = 0;
    std::vector<std::string> participants;
    int                    event_count    = 0;
    std::string            summary;
    std::vector<std::string> key_moments;
};

struct ChapterLog {
    int                                    chapter       = 0;
    std::string                            title;
    std::vector<SceneSummary>              scenes;
    std::vector<EventLogEntry>             events;
    int64_t                                start_timestamp = 0;
    int64_t                                end_timestamp   = 0;
    int                                    total_events  = 0;
    std::unordered_map<std::string, int>   character_appearances;
};

struct NovelLog {
    std::string                novel_title;
    std::string                source_world_model;
    int                        total_chapters       = 0;
    std::vector<ChapterLog>    chapters;
    int64_t                    generated_timestamp   = 0;
    int                        total_events         = 0;
    int                        total_characters     = 0;
};

void to_json(nlohmann::json& j, const WorldFlag& v);
void from_json(const nlohmann::json& j, WorldFlag& v);
void to_json(nlohmann::json& j, const NpcWorldState& v);
void from_json(const nlohmann::json& j, NpcWorldState& v);
void to_json(nlohmann::json& j, const RelationWorldState& v);
void from_json(const nlohmann::json& j, RelationWorldState& v);
void to_json(nlohmann::json& j, const EventLogEntry& v);
void from_json(const nlohmann::json& j, EventLogEntry& v);
void to_json(nlohmann::json& j, const SceneSummary& v);
void from_json(const nlohmann::json& j, SceneSummary& v);
void to_json(nlohmann::json& j, const ChapterLog& v);
void from_json(const nlohmann::json& j, ChapterLog& v);
void to_json(nlohmann::json& j, const NovelLog& v);
void from_json(const nlohmann::json& j, NovelLog& v);

class WorldStateManager {
public:
    static WorldStateManager& instance();

    void load_world(
        const std::string& name,
        const std::vector<std::pair<std::string, std::string>>& characters,
        const std::unordered_map<std::string, std::string>& locations,
        const std::string& timeline);

    void tick();
    void advance_chapter(int new_chapter);

    void record_event(
        const std::string& type,
        const std::string& content,
        float emotion_intensity = 0,
        const std::vector<WorldFlag>& flags = {},
        float narrative_weight = 0.5f);

    void record_npc_dialog(
        const std::string& speaker_id,
        const std::string& speaker_name,
        const std::string& listener_id,
        const std::string& content,
        const std::string& emotion = "neutral",
        float intensity = 0);

    void move_npc(const std::string& npc_id, const std::string& location_id);
    std::string get_npc_location(const std::string& npc_id) const;
    std::vector<WorldFlag> get_active_flags(const std::string& npc_id) const;

    void set_world_flag(
        const std::string& key,
        const std::string& value,
        const std::string& severity = "significant");

    NovelLog export_novel_log() const;

    static constexpr int   RELATION_TICK_INTERVAL = 5;
    static constexpr float RELATION_DECAY_RATE    = 0.01f;
    static constexpr int   PLOT_CHECK_INTERVAL    = 3;

private:
    WorldStateManager()  = default;
    ~WorldStateManager() = default;
    WorldStateManager(const WorldStateManager&)            = delete;
    WorldStateManager& operator=(const WorldStateManager&) = delete;

    std::string generate_event_id() const;
    int64_t     now_ms() const;
    void        decay_relations();
    void        check_plot_triggers();

    std::string                                         m_world_name;
    std::string                                         m_world_timeline;
    std::unordered_map<std::string, std::string>        m_locations;
    std::unordered_map<std::string, NpcWorldState>      m_npcs;
    std::vector<RelationWorldState>                     m_relations;

    int     m_current_chapter       = 1;
    int64_t m_tick_count            = 0;
    int64_t m_relation_tick_counter = 0;
    int64_t m_plot_tick_counter     = 0;

    std::vector<EventLogEntry>  m_current_scene_events;
    ChapterLog                  m_current_chapter_log;
    std::vector<ChapterLog>     m_completed_chapters;
    int                         m_current_scene_index = 0;
    int64_t                     m_scene_start_time    = 0;
};
