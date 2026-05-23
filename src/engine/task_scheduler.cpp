#include "engine/task_scheduler.h"
#include <sstream>
#include <chrono>
#include <ctime>

namespace npc {

TaskScheduler& TaskScheduler::instance() {
    static TaskScheduler inst;
    return inst;
}

void TaskScheduler::configure(int tick_interval_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tick_interval_ms = tick_interval_ms;
}

std::string TaskScheduler::generateId() {
    return "tsk_" + std::to_string(m_next_id++);
}

int64_t TaskScheduler::nowMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string TaskScheduler::scheduleOnce(const std::string& name,
                                         int64_t delay_ms,
                                         const std::string& action,
                                         const std::string& params) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_scheduled++;

    ScheduledTask task;
    task.id = generateId();
    task.name = name;
    task.type = TaskType::ONCE;
    task.trigger_at = nowMs() + delay_ms;
    task.action = action;
    task.action_params = params;
    task.created_at = nowMs();
    task.next_run_at = task.trigger_at;

    m_tasks.push_back(task);
    return task.id;
}

std::string TaskScheduler::scheduleRepeating(const std::string& name,
                                              int64_t interval_ms,
                                              const std::string& action,
                                              const std::string& params,
                                              int max_repeats) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_scheduled++;

    ScheduledTask task;
    task.id = generateId();
    task.name = name;
    task.type = TaskType::REPEATING;
    task.interval_ms = interval_ms;
    task.max_repeats = max_repeats;
    task.trigger_at = nowMs() + interval_ms;
    task.action = action;
    task.action_params = params;
    task.created_at = nowMs();
    task.next_run_at = task.trigger_at;

    m_tasks.push_back(task);
    return task.id;
}

std::string TaskScheduler::scheduleAt(const std::string& name,
                                       int64_t unix_timestamp_ms,
                                       const std::string& action,
                                       const std::string& params) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_scheduled++;

    ScheduledTask task;
    task.id = generateId();
    task.name = name;
    task.type = TaskType::ONCE;
    task.trigger_at = unix_timestamp_ms;
    task.action = action;
    task.action_params = params;
    task.created_at = nowMs();
    task.next_run_at = task.trigger_at;

    m_tasks.push_back(task);
    return task.id;
}

std::string TaskScheduler::scheduleConditional(const std::string& name,
                                                const std::string& condition,
                                                const std::string& action,
                                                const std::string& params,
                                                int64_t check_interval_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_scheduled++;

    ScheduledTask task;
    task.id = generateId();
    task.name = name;
    task.type = TaskType::CONDITIONAL;
    task.condition_expr = condition;
    task.check_interval_ms = check_interval_ms;
    task.action = action;
    task.action_params = params;
    task.created_at = nowMs();
    task.last_check_at = nowMs();
    task.next_run_at = nowMs() + check_interval_ms;

    m_tasks.push_back(task);
    return task.id;
}

bool TaskScheduler::cancel(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& task : m_tasks) {
        if (task.id == task_id && task.status == TaskStatus::PENDING) {
            task.status = TaskStatus::CANCELLED;
            return true;
        }
    }
    return false;
}

bool TaskScheduler::cancelByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    bool found = false;
    for (auto& task : m_tasks) {
        if (task.name == name && task.status == TaskStatus::PENDING) {
            task.status = TaskStatus::CANCELLED;
            found = true;
        }
    }
    return found;
}

std::vector<ScheduledTask> TaskScheduler::pendingTasks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ScheduledTask> pending;
    for (const auto& task : m_tasks) {
        if (task.status == TaskStatus::PENDING) {
            pending.push_back(task);
        }
    }
    return pending;
}

std::vector<ScheduledTask> TaskScheduler::allTasks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<ScheduledTask>(m_tasks.begin(), m_tasks.end());
}

ScheduledTask* TaskScheduler::getTask(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& task : m_tasks) {
        if (task.id == task_id) return &task;
    }
    return nullptr;
}

void TaskScheduler::registerExecutor(const std::string& action, TaskExecutor executor) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_executors[action] = std::move(executor);
}

void TaskScheduler::registerConditionChecker(const std::string& name, ConditionChecker checker) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_condition_checkers[name] = std::move(checker);
}

void TaskScheduler::setMonitorCallback(std::function<void(const std::string&, double)> cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_monitor_cb = std::move(cb);
}

bool TaskScheduler::monitorValue(const std::string& label, double value,
                                  const std::string& condition, double threshold) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& m : m_monitors) {
        if (m.label == label) {
            m.value = value;
            bool triggered = false;
            if (condition == "gt" || condition == ">") triggered = value > threshold;
            else if (condition == "lt" || condition == "<") triggered = value < threshold;
            else if (condition == "ge" || condition == ">=") triggered = value >= threshold;
            else if (condition == "le" || condition == "<=") triggered = value <= threshold;
            else if (condition == "eq" || condition == "==") triggered = value == threshold;

            if (triggered) {
                int64_t now = nowMs();
                if (now - m.last_alert_at > 30000) {
                    m.last_alert_at = now;
                    if (m_monitor_cb) {
                        m_monitor_cb(label, value);
                    }
                }
            }
            return triggered;
        }
    }

    MonitorEntry entry;
    entry.label = label;
    entry.value = value;
    entry.condition = condition;
    entry.threshold = threshold;
    m_monitors.push_back(entry);
    m_stats.active_monitors = (int64_t)m_monitors.size();
    return false;
}

void TaskScheduler::executeTask(ScheduledTask& task) {
    task.status = TaskStatus::RUNNING;
    task.last_run_at = nowMs();
    task.run_count++;
    m_stats.total_executed++;

    if (task.action == "log" || task.action == "notify") {
        task.status = TaskStatus::COMPLETED;
        fprintf(stdout, "[TaskScheduler] 任务完成: %s (params: %s)\n",
                task.name.c_str(), task.action_params.c_str());
        return;
    }

    auto exec_it = m_executors.find(task.action);
    if (exec_it != m_executors.end() && exec_it->second) {
        bool ok = exec_it->second(task);
        if (ok) {
            task.status = TaskStatus::COMPLETED;
        } else {
            task.status = TaskStatus::FAILED;
            task.error_count++;
            task.last_error = "执行器返回失败";
            m_stats.total_failed++;
        }
    } else {
        task.status = TaskStatus::COMPLETED;
    }
}

bool TaskScheduler::evaluateCondition(const ScheduledTask& task) const {
    auto checker_it = m_condition_checkers.find(task.condition_expr);
    if (checker_it != m_condition_checkers.end() && checker_it->second) {
        return checker_it->second(task);
    }
    return false;
}

void TaskScheduler::tick() {
    std::lock_guard<std::mutex> lock(m_mutex);

    int64_t now = nowMs();

    for (auto& task : m_tasks) {
        if (task.status == TaskStatus::PENDING) {
            bool should_run = false;

            switch (task.type) {
                case TaskType::ONCE:
                    should_run = (now >= task.trigger_at);
                    break;
                case TaskType::REPEATING:
                    should_run = (now >= task.next_run_at);
                    break;
                case TaskType::CONDITIONAL:
                    if (now - task.last_check_at >= task.check_interval_ms) {
                        task.last_check_at = now;
                        should_run = evaluateCondition(task);
                    }
                    break;
            }

            if (should_run) {
                executeTask(task);

                if (task.type == TaskType::REPEATING && task.status == TaskStatus::COMPLETED) {
                    if (task.max_repeats < 0 || task.repeat_count < task.max_repeats) {
                        task.status = TaskStatus::PENDING;
                        task.next_run_at = now + task.interval_ms;
                        task.repeat_count++;
                    }
                }
            }
        }
    }

    m_stats.pending_count = 0;
    for (const auto& task : m_tasks) {
        if (task.status == TaskStatus::PENDING) m_stats.pending_count++;
    }

    m_stats.active_monitors = (int64_t)m_monitors.size();
    m_last_tick_at = now;
}

TaskScheduler::SchedulerStats TaskScheduler::stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    SchedulerStats s = m_stats;
    s.pending_count = 0;
    for (const auto& task : m_tasks) {
        if (task.status == TaskStatus::PENDING) s.pending_count++;
    }
    return s;
}

} // namespace npc