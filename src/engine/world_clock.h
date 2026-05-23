#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <chrono>

struct GameTime {
    int year   = 1;
    int month  = 1;
    int day    = 1;
    int hour   = 8;
    int minute = 0;

    bool operator<(const GameTime& o) const {
        if (year != o.year)   return year < o.year;
        if (month != o.month) return month < o.month;
        if (day != o.day)     return day < o.day;
        if (hour != o.hour)   return hour < o.hour;
        return minute < o.minute;
    }
    bool operator==(const GameTime& o) const {
        return year == o.year && month == o.month && day == o.day &&
               hour == o.hour && minute == o.minute;
    }
    bool operator!=(const GameTime& o) const { return !(*this == o); }
    bool operator<=(const GameTime& o) const { return *this < o || *this == o; }
    bool operator>(const GameTime& o) const  { return !(*this <= o); }
    bool operator>=(const GameTime& o) const { return !(*this < o); }

    int64_t to_minutes() const {
        return static_cast<int64_t>(minute)
             + static_cast<int64_t>(hour) * 60
             + static_cast<int64_t>(day) * 1440
             + static_cast<int64_t>(month) * 43200
             + static_cast<int64_t>(year) * 525600;
    }
    static GameTime from_minutes(int64_t mins);
    std::string to_string() const;
};

enum class GameEventType {
    Timed,
    Proximity,
    Broadcast,
    Custom
};

struct GameEvent {
    int64_t id;
    GameEventType type;
    std::string tag;
    GameTime trigger_time;
    int priority = 5;

    std::string npc_a;
    std::string npc_b;
    float proximity_threshold = 0.0f;

    std::string broadcast_tag;

    bool repeat = false;
    int repeat_interval_minutes = 0;

    std::string custom_data;

    bool operator<(const GameEvent& o) const {
        if (trigger_time != o.trigger_time)
            return trigger_time > o.trigger_time;
        return priority > o.priority;
    }
};

using EventCallback = std::function<void(const GameEvent&)>;

class WorldClock {
public:
    static WorldClock& instance();

    void configure(float timeScale);
    float timeScale() const { return m_timeScale; }

    void start();
    void pause();
    void resume();
    bool isPaused() const { return m_paused; }

    void tick();

    GameTime now() const;
    int64_t elapsedGameMinutes() const { return m_gameTime.to_minutes(); }
    double elapsedRealSeconds() const;

    int64_t schedule(const GameEvent& event);
    int64_t scheduleTimed(const GameTime& when, const std::string& tag,
                          int priority = 5, bool repeat = false,
                          int repeatIntervalMinutes = 0);
    int64_t scheduleProximity(const std::string& npcA, const std::string& npcB,
                               float threshold, const std::string& tag,
                               int priority = 5);
    int64_t scheduleBroadcast(const std::string& broadcastTag,
                               const std::string& tag, int priority = 5);
    bool cancel(int64_t eventId);

    void broadcast(const std::string& tag, const std::string& customData = "");

    void onEvent(const std::string& tag, EventCallback cb);
    void removeEventCallback(const std::string& tag);

    void advanceTo(const GameTime& target);
    void advanceMinutes(int minutes);

    void setTime(const GameTime& t);

    size_t pendingEventCount() const;

private:
    WorldClock() = default;
    WorldClock(const WorldClock&) = delete;
    WorldClock& operator=(const WorldClock&) = delete;

    void processReadyEvents();
    void checkProximityEvents();
    void fireEvent(const GameEvent& event);
    void addGameMinutes(int minutes);

    float m_timeScale = 60.0f;

    GameTime m_gameTime;

    std::chrono::steady_clock::time_point m_lastTick;
    bool m_paused = false;
    bool m_running = false;
    int64_t m_nextEventId = 1;

    std::priority_queue<GameEvent> m_eventQueue;
    std::vector<GameEvent> m_proximityEvents;
    std::vector<GameEvent> m_broadcastEvents;
    std::vector<GameEvent> m_repeatingEvents;

    mutable std::mutex m_mutex;

    std::unordered_map<std::string, std::vector<EventCallback>> m_callbacks;
};
