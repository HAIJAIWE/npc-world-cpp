#pragma once

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <atomic>

namespace npc {

enum class TaskType : uint8_t {
    ONCE,
    REPEATING,
    CONDITIONAL
};

enum class TaskStatus : uint8_t {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED
};

struct ScheduledTask {
    std::string id;
    std::string name;
    TaskType type = TaskType::ONCE;
    TaskStatus status = TaskStatus::PENDING;

    int64_t trigger_at = 0;
    int64_t interval_ms = 0;
    int repeat_count = 0;
    int max_repeats = 1;

    std::string condition_expr;
    int64_t last_check_at = 0;
    int64_t check_interval_ms = 5000;

    std::string action;
    std::string action_params;

    int64_t created_at = 0;
    int64_t last_run_at = 0;
    int64_t next_run_at = 0;
    int run_count = 0;
    int error_count = 0;
    std::string last_error;
};

using TaskExecutor = std::function<bool(ScheduledTask& task)>;
using ConditionChecker = std::function<bool(const ScheduledTask& task)>;

class TaskScheduler {
public:
    static TaskScheduler& instance();

    void configure(int tick_interval_ms = 1000);

    std::string scheduleOnce(const std::string& name,
                              int64_t delay_ms,
                              const std::string& action,
                              const std::string& params = "");

    std::string scheduleRepeating(const std::string& name,
                                   int64_t interval_ms,
                                   const std::string& action,
                                   const std::string& params = "",
                                   int max_repeats = -1);

    std::string scheduleAt(const std::string& name,
                            int64_t unix_timestamp_ms,
                            const std::string& action,
                            const std::string& params = "");

    std::string scheduleConditional(const std::string& name,
                                     const std::string& condition,
                                     const std::string& action,
                                     const std::string& params = "",
                                     int64_t check_interval_ms = 5000);

    bool cancel(const std::string& task_id);
    bool cancelByName(const std::string& name);

    std::vector<ScheduledTask> pendingTasks() const;
    std::vector<ScheduledTask> allTasks() const;
    ScheduledTask* getTask(const std::string& task_id);

    void tick();

    void registerExecutor(const std::string& action, TaskExecutor executor);
    void registerConditionChecker(const std::string& name, ConditionChecker checker);

    bool monitorValue(const std::string& label, double value,
                       const std::string& condition, double threshold);

    void setMonitorCallback(std::function<void(const std::string&, double)> cb);

    struct SchedulerStats {
        int64_t total_scheduled = 0;
        int64_t total_executed = 0;
        int64_t total_failed = 0;
        int64_t pending_count = 0;
        int64_t active_monitors = 0;
    };
    SchedulerStats stats() const;

private:
    TaskScheduler() = default;
    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    std::string generateId();
    int64_t nowMs() const;
    void executeTask(ScheduledTask& task);
    bool evaluateCondition(const ScheduledTask& task) const;

    std::deque<ScheduledTask> m_tasks;
    std::unordered_map<std::string, TaskExecutor> m_executors;
    std::unordered_map<std::string, ConditionChecker> m_condition_checkers;
    std::function<void(const std::string&, double)> m_monitor_cb;

    struct MonitorEntry {
        std::string label;
        double value = 0.0;
        std::string condition;
        double threshold = 0.0;
        int64_t last_alert_at = 0;
        bool active = true;
    };
    std::vector<MonitorEntry> m_monitors;

    int m_tick_interval_ms = 1000;
    int64_t m_next_id = 1;
    int64_t m_last_tick_at = 0;
    SchedulerStats m_stats;
    mutable std::mutex m_mutex;
};

} // namespace npc
