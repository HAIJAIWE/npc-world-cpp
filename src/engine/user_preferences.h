#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>

namespace npc {
class Database;
}

class UserPreferences {
public:
    struct PreferenceItem {
        std::string key;
        std::string value;
        float confidence = 0.5f;
        int64_t updated_at = 0;
    };

    static UserPreferences& instance();

    bool initialize(std::shared_ptr<npc::Database> db);

    void set(const std::string& key, const std::string& value, float confidence = 0.8f);
    std::string get(const std::string& key, const std::string& default_val = "") const;
    std::vector<PreferenceItem> all() const;

    void extractFromMessage(const std::string& user_message);
    std::string formatForPrompt() const;

    struct PrefStats {
        int total_prefs = 0;
        int high_confidence = 0;
    };
    PrefStats stats() const;

private:
    UserPreferences() = default;
    std::shared_ptr<npc::Database> m_db;
    bool m_initialized = false;

    void loadFromDb();
    void saveToDb(const std::string& key, const std::string& value, float confidence);
    void ensureTable();
};
