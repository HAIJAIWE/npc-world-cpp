#include "engine/world_state.h"
#include <algorithm>
#include <chrono>
#include <random>

using json = nlohmann::json;

static std::string generate_random_id() {
    static thread_local std::mt19937_64 rng(
        static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    static const char hex[] = "0123456789abcdef";
    std::string id(16, '\0');
    for (int i = 0; i < 16; ++i) id[i] = hex[rng() & 0xF];
    return id;
}

static int64_t current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void to_json(json& j, const WorldFlag& v) {
    j = json{{"key", v.key}, {"value", v.value}, {"severity", v.severity}, {"chapter_set", v.chapter_set}};
}
void from_json(const json& j, WorldFlag& v) {
    j.at("key").get_to(v.key); j.at("value").get_to(v.value);
    j.at("severity").get_to(v.severity); j.at("chapter_set").get_to(v.chapter_set);
}

void to_json(json& j, const NpcWorldState& v) {
    j = json{{"npc_id", v.npc_id}, {"npc_name", v.npc_name},
             {"current_location_id", v.current_location_id}, {"current_chapter", v.current_chapter},
             {"active_flags", v.active_flags}, {"last_tick_timestamp", v.last_tick_timestamp}};
}
void from_json(const json& j, NpcWorldState& v) {
    j.at("npc_id").get_to(v.npc_id); j.at("npc_name").get_to(v.npc_name);
    j.at("current_location_id").get_to(v.current_location_id); j.at("current_chapter").get_to(v.current_chapter);
    j.at("active_flags").get_to(v.active_flags); j.at("last_tick_timestamp").get_to(v.last_tick_timestamp);
}

void to_json(json& j, const RelationWorldState& v) {
    j = json{{"npc_a", v.npc_a}, {"npc_b", v.npc_b}, {"affection_trend", v.affection_trend},
             {"last_interaction_timestamp", v.last_interaction_timestamp}, {"tension_level", v.tension_level}};
}
void from_json(const json& j, RelationWorldState& v) {
    j.at("npc_a").get_to(v.npc_a); j.at("npc_b").get_to(v.npc_b);
    j.at("affection_trend").get_to(v.affection_trend);
    j.at("last_interaction_timestamp").get_to(v.last_interaction_timestamp);
    j.at("tension_level").get_to(v.tension_level);
}

void to_json(json& j, const EventLogEntry& v) {
    j = json{{"id", v.id}, {"timestamp", v.timestamp}, {"type", v.type},
             {"speaker", v.speaker}, {"listener", v.listener}, {"content", v.content},
             {"emotion", v.emotion}, {"emotion_intensity", v.emotion_intensity},
             {"location_id", v.location_id}, {"location_name", v.location_name},
             {"chapter", v.chapter}, {"scene_index", v.scene_index},
             {"world_flags", v.world_flags}, {"affected_npcs", v.affected_npcs},
             {"narrative_weight", v.narrative_weight}};
}
void from_json(const json& j, EventLogEntry& v) {
    j.at("id").get_to(v.id); j.at("timestamp").get_to(v.timestamp); j.at("type").get_to(v.type);
    j.at("speaker").get_to(v.speaker); j.at("listener").get_to(v.listener);
    j.at("content").get_to(v.content); j.at("emotion").get_to(v.emotion);
    j.at("emotion_intensity").get_to(v.emotion_intensity); j.at("location_id").get_to(v.location_id);
    j.at("location_name").get_to(v.location_name); j.at("chapter").get_to(v.chapter);
    j.at("scene_index").get_to(v.scene_index); j.at("world_flags").get_to(v.world_flags);
    j.at("affected_npcs").get_to(v.affected_npcs); j.at("narrative_weight").get_to(v.narrative_weight);
}

void to_json(json& j, const SceneSummary& v) {
    j = json{{"scene_index", v.scene_index}, {"chapter", v.chapter}, {"location_id", v.location_id},
             {"start_timestamp", v.start_timestamp}, {"end_timestamp", v.end_timestamp},
             {"participants", v.participants}, {"event_count", v.event_count},
             {"summary", v.summary}, {"key_moments", v.key_moments}};
}
void from_json(const json& j, SceneSummary& v) {
    j.at("scene_index").get_to(v.scene_index); j.at("chapter").get_to(v.chapter);
    j.at("location_id").get_to(v.location_id); j.at("start_timestamp").get_to(v.start_timestamp);
    j.at("end_timestamp").get_to(v.end_timestamp); j.at("participants").get_to(v.participants);
    j.at("event_count").get_to(v.event_count); j.at("summary").get_to(v.summary);
    j.at("key_moments").get_to(v.key_moments);
}

void to_json(json& j, const ChapterLog& v) {
    j = json{{"chapter", v.chapter}, {"title", v.title}, {"scenes", v.scenes},
             {"events", v.events}, {"start_timestamp", v.start_timestamp},
             {"end_timestamp", v.end_timestamp}, {"total_events", v.total_events},
             {"character_appearances", v.character_appearances}};
}
void from_json(const json& j, ChapterLog& v) {
    j.at("chapter").get_to(v.chapter); j.at("title").get_to(v.title);
    j.at("scenes").get_to(v.scenes); j.at("events").get_to(v.events);
    j.at("start_timestamp").get_to(v.start_timestamp); j.at("end_timestamp").get_to(v.end_timestamp);
    j.at("total_events").get_to(v.total_events);
    j.at("character_appearances").get_to(v.character_appearances);
}

void to_json(json& j, const NovelLog& v) {
    j = json{{"novel_title", v.novel_title}, {"source_world_model", v.source_world_model},
             {"total_chapters", v.total_chapters}, {"chapters", v.chapters},
             {"generated_timestamp", v.generated_timestamp}, {"total_events", v.total_events},
             {"total_characters", v.total_characters}};
}
void from_json(const json& j, NovelLog& v) {
    j.at("novel_title").get_to(v.novel_title); j.at("source_world_model").get_to(v.source_world_model);
    j.at("total_chapters").get_to(v.total_chapters); j.at("chapters").get_to(v.chapters);
    j.at("generated_timestamp").get_to(v.generated_timestamp);
    j.at("total_events").get_to(v.total_events); j.at("total_characters").get_to(v.total_characters);
}

WorldStateManager& WorldStateManager::instance() {
    static WorldStateManager s_instance;
    return s_instance;
}

std::string WorldStateManager::generate_event_id() const { return generate_random_id(); }
int64_t WorldStateManager::now_ms() const { return current_time_ms(); }

void WorldStateManager::decay_relations() {
    for (auto& rel : m_relations) {
        rel.affection_trend = std::clamp(rel.affection_trend - RELATION_DECAY_RATE, 0.0f, 1.0f);
    }
}

void WorldStateManager::check_plot_triggers() {
    for (const auto& [id, npc] : m_npcs) {
        for (const auto& flag : npc.active_flags) {
            if (flag.severity == "critical") break;
        }
    }
}

void WorldStateManager::load_world(
    const std::string& name,
    const std::vector<std::pair<std::string, std::string>>& characters,
    const std::unordered_map<std::string, std::string>& locations,
    const std::string& timeline) {
    m_world_name = name;
    m_world_timeline = timeline;
    m_locations = locations;
    m_current_chapter = 1;
    m_tick_count = 0;
    m_relation_tick_counter = 0;
    m_plot_tick_counter = 0;
    m_current_scene_index = 0;
    m_scene_start_time = 0;
    m_npcs.clear();
    m_relations.clear();
    m_current_scene_events.clear();
    m_current_chapter_log = ChapterLog{};
    m_completed_chapters.clear();

    for (const auto& [id, npc_name] : characters) {
        NpcWorldState nws;
        nws.npc_id = id;
        nws.npc_name = npc_name;
        nws.current_chapter = 1;
        nws.last_tick_timestamp = now_ms();
        if (!m_locations.empty()) nws.current_location_id = m_locations.begin()->first;
        m_npcs[id] = std::move(nws);
    }
    m_current_chapter_log.chapter = 1;
    m_current_chapter_log.start_timestamp = now_ms();
}

void WorldStateManager::tick() {
    ++m_tick_count;
    int64_t t = now_ms();
    for (auto& [id, npc] : m_npcs) npc.last_tick_timestamp = t;

    ++m_relation_tick_counter;
    if (m_relation_tick_counter >= RELATION_TICK_INTERVAL) {
        m_relation_tick_counter = 0;
        decay_relations();
    }
    ++m_plot_tick_counter;
    if (m_plot_tick_counter >= PLOT_CHECK_INTERVAL) {
        m_plot_tick_counter = 0;
        check_plot_triggers();
    }
}

void WorldStateManager::record_event(const std::string& type, const std::string& content,
                                      float emotion_intensity, const std::vector<WorldFlag>& flags,
                                      float narrative_weight) {
    EventLogEntry entry;
    entry.id = generate_event_id();
    entry.timestamp = now_ms();
    entry.type = type;
    entry.content = content;
    entry.emotion_intensity = emotion_intensity;
    entry.world_flags = flags;
    entry.narrative_weight = narrative_weight;
    entry.chapter = m_current_chapter;
    entry.scene_index = m_current_scene_index;
    m_current_scene_events.push_back(std::move(entry));
}

void WorldStateManager::record_npc_dialog(const std::string& speaker_id, const std::string& speaker_name,
                                           const std::string& listener_id, const std::string& content,
                                           const std::string& emotion, float intensity) {
    int64_t ts = now_ms();
    EventLogEntry entry;
    entry.id = generate_event_id();
    entry.timestamp = ts;
    entry.type = "dialog";
    entry.speaker = speaker_id;
    entry.listener = listener_id;
    entry.content = content;
    entry.emotion = emotion;
    entry.emotion_intensity = intensity;
    entry.chapter = m_current_chapter;
    entry.scene_index = m_current_scene_index;

    auto npc_it = m_npcs.find(speaker_id);
    if (npc_it != m_npcs.end()) {
        entry.location_id = npc_it->second.current_location_id;
        auto loc_it = m_locations.find(npc_it->second.current_location_id);
        if (loc_it != m_locations.end()) entry.location_name = loc_it->second;
    }
    entry.affected_npcs.push_back(speaker_id);
    entry.affected_npcs.push_back(listener_id);
    m_current_scene_events.push_back(std::move(entry));

    if (speaker_id == listener_id) return;

    bool found = false;
    for (auto& rel : m_relations) {
        if ((rel.npc_a == speaker_id && rel.npc_b == listener_id) ||
            (rel.npc_a == listener_id && rel.npc_b == speaker_id)) {
            rel.affection_trend = std::clamp(rel.affection_trend + 0.02f, 0.0f, 1.0f);
            rel.last_interaction_timestamp = ts;
            found = true;
            break;
        }
    }
    if (!found) {
        RelationWorldState rel;
        rel.npc_a = speaker_id;
        rel.npc_b = listener_id;
        rel.affection_trend = 0.5f;
        rel.last_interaction_timestamp = ts;
        rel.tension_level = 0.0f;
        m_relations.push_back(std::move(rel));
    }
}

void WorldStateManager::move_npc(const std::string& npc_id, const std::string& location_id) {
    auto it = m_npcs.find(npc_id);
    if (it == m_npcs.end()) return;

    std::string old_loc_id = it->second.current_location_id;
    it->second.current_location_id = location_id;

    auto old_it = m_locations.find(old_loc_id);
    std::string old_loc_name = (old_it != m_locations.end()) ? old_it->second : old_loc_id;
    auto new_it = m_locations.find(location_id);
    std::string new_loc_name = (new_it != m_locations.end()) ? new_it->second : location_id;

    EventLogEntry entry;
    entry.id = generate_event_id();
    entry.timestamp = now_ms();
    entry.type = "scene_change";
    entry.speaker = npc_id;
    entry.content = it->second.npc_name + " moved from " + old_loc_name + " to " + new_loc_name;
    entry.chapter = m_current_chapter;
    entry.scene_index = m_current_scene_index;
    entry.location_id = location_id;
    entry.location_name = new_loc_name;
    entry.affected_npcs.push_back(npc_id);
    m_current_scene_events.push_back(std::move(entry));
}

void WorldStateManager::advance_chapter(int new_chapter) {
    int64_t ts = now_ms();

    m_current_chapter_log.end_timestamp = ts;
    m_current_chapter_log.events = std::move(m_current_scene_events);
    m_current_scene_events.clear();
    m_current_chapter_log.total_events = static_cast<int>(m_current_chapter_log.events.size());
    m_current_chapter_log.character_appearances.clear();
    for (const auto& evt : m_current_chapter_log.events) {
        for (const auto& id : evt.affected_npcs) {
            m_current_chapter_log.character_appearances[id]++;
        }
    }
    m_completed_chapters.push_back(std::move(m_current_chapter_log));

    m_current_chapter = new_chapter;
    m_current_scene_index = 0;
    m_scene_start_time = ts;
    m_current_chapter_log = ChapterLog{};
    m_current_chapter_log.chapter = new_chapter;
    m_current_chapter_log.start_timestamp = ts;

    for (auto& [id, npc] : m_npcs) {
        if (npc.current_chapter < new_chapter) npc.current_chapter = new_chapter;
    }

    EventLogEntry entry;
    entry.id = generate_event_id();
    entry.timestamp = ts;
    entry.type = "system";
    entry.content = "Chapter " + std::to_string(new_chapter) + " begins";
    entry.chapter = new_chapter;
    entry.scene_index = m_current_scene_index;
    m_current_scene_events.push_back(std::move(entry));
}

std::string WorldStateManager::get_npc_location(const std::string& npc_id) const {
    auto npc_it = m_npcs.find(npc_id);
    if (npc_it == m_npcs.end()) return {};
    auto loc_it = m_locations.find(npc_it->second.current_location_id);
    return (loc_it != m_locations.end()) ? loc_it->second : "";
}

std::vector<WorldFlag> WorldStateManager::get_active_flags(const std::string& npc_id) const {
    auto it = m_npcs.find(npc_id);
    if (it == m_npcs.end()) return {};
    return it->second.active_flags;
}

void WorldStateManager::set_world_flag(const std::string& key, const std::string& value,
                                        const std::string& severity) {
    WorldFlag flag{key, value, severity, m_current_chapter};
    for (auto& [id, state] : m_npcs) {
        bool found = false;
        for (auto& f : state.active_flags) {
            if (f.key == key) { f = flag; found = true; break; }
        }
        if (!found) state.active_flags.push_back(flag);
    }
}

NovelLog WorldStateManager::export_novel_log() const {
    NovelLog log;
    log.novel_title = m_world_name.empty() ? "untitled" : m_world_name;
    log.generated_timestamp = current_time_ms();

    std::unordered_map<int, std::vector<EventLogEntry>> chapters_map;
    for (const auto& e : m_completed_chapters) {
        chapters_map[e.chapter] = e.events;
    }
    if (!m_current_scene_events.empty()) {
        auto& ce = chapters_map[m_current_chapter];
        ce.insert(ce.end(), m_current_scene_events.begin(), m_current_scene_events.end());
    }

    for (auto& [ch, events] : chapters_map) {
        ChapterLog cl;
        cl.chapter = ch;
        cl.title = "Chapter " + std::to_string(ch);
        cl.events = events;
        cl.total_events = static_cast<int>(events.size());
        cl.start_timestamp = events.empty() ? 0 : events.front().timestamp;
        cl.end_timestamp = events.empty() ? 0 : events.back().timestamp;
        for (const auto& e : events) {
            for (const auto& n : e.affected_npcs) cl.character_appearances[n]++;
        }

        std::vector<EventLogEntry> current_scene;
        int scene_idx = 0;
        for (const auto& e : events) {
            if (e.type == "scene_change" && !current_scene.empty()) {
                SceneSummary sm;
                sm.scene_index = scene_idx++;
                sm.chapter = ch;
                sm.location_id = current_scene.front().location_id;
                sm.start_timestamp = current_scene.front().timestamp;
                sm.end_timestamp = current_scene.back().timestamp;
                sm.event_count = static_cast<int>(current_scene.size());
                for (const auto& ce : current_scene)
                    for (const auto& n : ce.affected_npcs) sm.participants.push_back(n);
                std::sort(sm.participants.begin(), sm.participants.end());
                sm.participants.erase(std::unique(sm.participants.begin(), sm.participants.end()), sm.participants.end());

                std::string dialog_summary;
                for (const auto& ce : current_scene) {
                    if (ce.type == "dialog") {
                        if (!dialog_summary.empty()) dialog_summary += "; ";
                        dialog_summary += ce.speaker + ": " + ce.content.substr(0, std::min<size_t>(50, ce.content.size()));
                    }
                }
                sm.summary = dialog_summary.empty() ? "(no dialog)" : dialog_summary;
                for (const auto& ce : current_scene) {
                    if (ce.narrative_weight > 0.7f)
                        sm.key_moments.push_back(ce.content.substr(0, std::min<size_t>(100, ce.content.size())));
                }
                cl.scenes.push_back(std::move(sm));
                current_scene.clear();
            }
            current_scene.push_back(e);
        }
        if (!current_scene.empty()) {
            SceneSummary sm;
            sm.scene_index = scene_idx;
            sm.chapter = ch;
            sm.event_count = static_cast<int>(current_scene.size());
            sm.start_timestamp = current_scene.front().timestamp;
            sm.end_timestamp = current_scene.back().timestamp;
            for (const auto& ce : current_scene)
                for (const auto& n : ce.affected_npcs) sm.participants.push_back(n);
            sm.summary = "(no dialog)";
            cl.scenes.push_back(std::move(sm));
        }
        log.chapters.push_back(std::move(cl));
    }

    std::sort(log.chapters.begin(), log.chapters.end(),
              [](const ChapterLog& a, const ChapterLog& b) { return a.chapter < b.chapter; });

    log.total_chapters = static_cast<int>(log.chapters.size());
    log.total_characters = static_cast<int>(m_npcs.size());
    log.total_events = 0;
    for (const auto& c : log.chapters) log.total_events += c.total_events;

    return log;
}