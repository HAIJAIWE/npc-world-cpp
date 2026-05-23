#include "engine/world_clock.h"
#include "engine/world_state.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

GameTime GameTime::from_minutes(int64_t mins) {
    GameTime t;
    int64_t m = mins;
    t.year   = static_cast<int>(m / 525600); m %= 525600;
    t.month  = static_cast<int>(m / 43200) + 1; m %= 43200;
    t.day    = static_cast<int>(m / 1440) + 1; m %= 1440;
    t.hour   = static_cast<int>(m / 60); m %= 60;
    t.minute = static_cast<int>(m);
    while (t.month > 12) { t.month -= 12; t.year++; }
    while (t.day > 30)   { t.day -= 30; t.month++; if (t.month > 12) { t.month -= 12; t.year++; } }
    if (t.month < 1) t.month = 1;
    if (t.day < 1) t.day = 1;
    return t;
}

std::string GameTime::to_string() const {
    std::ostringstream oss;
    oss << "Y" << year << "/M" << std::setfill('0') << std::setw(2) << month
        << "/D" << std::setw(2) << day
        << " " << std::setw(2) << hour << ":" << std::setw(2) << minute;
    return oss.str();
}

WorldClock& WorldClock::instance() {
    static WorldClock clock;
    return clock;
}

void WorldClock::configure(float timeScale) {
    m_timeScale = timeScale;
}

void WorldClock::start() {
    m_running = true;
    m_paused = false;
    m_lastTick = std::chrono::steady_clock::now();
    fprintf(stdout, "[WorldClock] Started at %s, scale=%.1fx\n",
        now().to_string().c_str(), m_timeScale);
}

void WorldClock::pause() {
    m_paused = true;
}

void WorldClock::resume() {
    if (m_paused) {
        m_paused = false;
        m_lastTick = std::chrono::steady_clock::now();
    }
}

void WorldClock::tick() {
    if (m_paused || !m_running) return;

    auto now_real = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now_real - m_lastTick).count();

    double gameMinutesPassed = elapsed * m_timeScale / 60.0;
    int wholeMinutes = static_cast<int>(gameMinutesPassed);

    if (wholeMinutes > 0) {
        m_lastTick = now_real;
        addGameMinutes(wholeMinutes);
        processReadyEvents();
        checkProximityEvents();

        WorldStateManager::instance().tick();
    }
}

GameTime WorldClock::now() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_gameTime;
}

void WorldClock::addGameMinutes(int minutes) {
    int64_t total = m_gameTime.to_minutes() + minutes;
    m_gameTime = GameTime::from_minutes(total);
}

double WorldClock::elapsedRealSeconds() const {
    return m_gameTime.to_minutes() * 60.0 / m_timeScale;
}

int64_t WorldClock::schedule(const GameEvent& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    GameEvent e = event;
    e.id = m_nextEventId++;
    m_eventQueue.push(e);
    return e.id;
}

int64_t WorldClock::scheduleTimed(const GameTime& when, const std::string& tag,
                                   int priority, bool repeat, int repeatIntervalMinutes) {
    GameEvent e;
    e.type = GameEventType::Timed;
    e.trigger_time = when;
    e.tag = tag;
    e.priority = priority;
    e.repeat = repeat;
    e.repeat_interval_minutes = repeatIntervalMinutes;
    return schedule(e);
}

int64_t WorldClock::scheduleProximity(const std::string& npcA, const std::string& npcB,
                                       float threshold, const std::string& tag, int priority) {
    std::lock_guard<std::mutex> lock(m_mutex);
    GameEvent e;
    e.id = m_nextEventId++;
    e.type = GameEventType::Proximity;
    e.npc_a = npcA;
    e.npc_b = npcB;
    e.proximity_threshold = threshold;
    e.tag = tag;
    e.priority = priority;
    e.trigger_time = m_gameTime;
    m_proximityEvents.push_back(e);
    return e.id;
}

int64_t WorldClock::scheduleBroadcast(const std::string& broadcastTag,
                                       const std::string& tag, int priority) {
    std::lock_guard<std::mutex> lock(m_mutex);
    GameEvent e;
    e.id = m_nextEventId++;
    e.type = GameEventType::Broadcast;
    e.broadcast_tag = broadcastTag;
    e.tag = tag;
    e.priority = priority;
    e.trigger_time = m_gameTime;
    m_broadcastEvents.push_back(e);
    return e.id;
}

bool WorldClock::cancel(int64_t eventId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_proximityEvents.erase(
        std::remove_if(m_proximityEvents.begin(), m_proximityEvents.end(),
                       [eventId](const GameEvent& e) { return e.id == eventId; }),
        m_proximityEvents.end());
    m_broadcastEvents.erase(
        std::remove_if(m_broadcastEvents.begin(), m_broadcastEvents.end(),
                       [eventId](const GameEvent& e) { return e.id == eventId; }),
        m_broadcastEvents.end());
    return true;
}

void WorldClock::broadcast(const std::string& tag, const std::string& customData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& e : m_broadcastEvents) {
        if (e.broadcast_tag == tag) {
            e.custom_data = customData;
            fireEvent(e);
        }
    }
}

void WorldClock::onEvent(const std::string& tag, EventCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks[tag].push_back(std::move(cb));
}

void WorldClock::removeEventCallback(const std::string& tag) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.erase(tag);
}

void WorldClock::advanceTo(const GameTime& target) {
    int64_t targetMins = target.to_minutes();
    int64_t current = m_gameTime.to_minutes();
    if (targetMins <= current) return;
    int diff = static_cast<int>(targetMins - current);
    addGameMinutes(diff);
    processReadyEvents();
    checkProximityEvents();
}

void WorldClock::advanceMinutes(int minutes) {
    addGameMinutes(minutes);
    processReadyEvents();
    checkProximityEvents();
}

void WorldClock::setTime(const GameTime& t) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_gameTime = t;
}

size_t WorldClock::pendingEventCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_eventQueue.size();
}

void WorldClock::processReadyEvents() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<GameEvent> processed;

    while (!m_eventQueue.empty()) {
        GameEvent e = m_eventQueue.top();
        if (e.trigger_time > m_gameTime) break;
        m_eventQueue.pop();

        fireEvent(e);

        if (e.repeat) {
            e.trigger_time = GameTime::from_minutes(
                e.trigger_time.to_minutes() + e.repeat_interval_minutes);
            m_eventQueue.push(e);
        }
    }
}

void WorldClock::checkProximityEvents() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& wsm = WorldStateManager::instance();

    for (auto& e : m_proximityEvents) {
        std::string locA = wsm.get_npc_location(e.npc_a);
        std::string locB = wsm.get_npc_location(e.npc_b);
        if (!locA.empty() && locA == locB) {
            fireEvent(e);
        }
    }
}

void WorldClock::fireEvent(const GameEvent& event) {
    fprintf(stdout, "[WorldClock] Event fired: %s (type=%d)\n",
        event.tag.c_str(), static_cast<int>(event.type));

    auto it = m_callbacks.find(event.tag);
    if (it != m_callbacks.end()) {
        for (auto& cb : it->second) {
            if (cb) cb(event);
        }
    }
}