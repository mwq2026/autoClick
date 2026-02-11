#include "core/Scheduler.h"
#include "core/Logger.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>
#include <windows.h>

Scheduler::Scheduler() = default;
Scheduler::~Scheduler() { Stop(); }

void Scheduler::Start(ActionCallback callback) {
    if (running_.load()) return;
    callback_ = std::move(callback);
    running_.store(true);
    worker_ = std::thread([this] { ThreadMain(); });
    LOG_INFO("Scheduler::Start", "Scheduler started");
}

void Scheduler::Stop() {
    running_.store(false);
    if (worker_.joinable()) worker_.join();
    LOG_INFO("Scheduler::Stop", "Scheduler stopped");
}

bool Scheduler::IsRunning() const { return running_.load(); }

int Scheduler::AddTask(const ScheduledTask& task) {
    std::scoped_lock lock(mutex_);
    ScheduledTask t = task;
    t.id = nextId_++;
    t.runCount = 0;
    t.failCount = 0;
    t.finished = false;
    t.createdTime = NowEpochSeconds();
    t.status = t.enabled ? TaskStatus::Waiting : TaskStatus::Disabled;
    ComputeNextRun(t);
    tasks_.push_back(t);
    LOG_INFO("Scheduler::AddTask", "Added task id=%d name='%s'", t.id, t.name.c_str());
    return t.id;
}

void Scheduler::RemoveTask(int id) {
    std::scoped_lock lock(mutex_);
    tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
        [id](const ScheduledTask& t) { return t.id == id; }), tasks_.end());
    LOG_INFO("Scheduler::RemoveTask", "Removed task id=%d", id);
}

void Scheduler::UpdateTask(const ScheduledTask& task) {
    std::scoped_lock lock(mutex_);
    for (auto& t : tasks_) {
        if (t.id == task.id) {
            t.name = task.name;
            t.description = task.description;
            t.enabled = task.enabled;
            t.priority = task.priority;
            t.type = task.type;
            t.triggerTime = task.triggerTime;
            t.dateStr = task.dateStr;
            t.timeStr = task.timeStr;
            t.interval = task.interval;
            t.unit = task.unit;
            t.maxRuns = task.maxRuns;
            t.startDelaySec = task.startDelaySec;
            t.windowStartHour = task.windowStartHour;
            t.windowEndHour = task.windowEndHour;
            t.retryCount = task.retryCount;
            t.retryDelaySec = task.retryDelaySec;
            t.actionMode = task.actionMode;
            t.actionPath = task.actionPath;
            t.actionSpeed = task.actionSpeed;
            t.actionBlockInput = task.actionBlockInput;
            ComputeNextRun(t);
            break;
        }
    }
}

void Scheduler::SetTaskEnabled(int id, bool enabled) {
    std::scoped_lock lock(mutex_);
    for (auto& t : tasks_) {
        if (t.id == id) {
            t.enabled = enabled;
            t.status = enabled ? TaskStatus::Waiting : TaskStatus::Disabled;
            if (enabled && t.finished) {
                // Re-enable resets finished state for periodic
                if (t.type == TaskType::Periodic) { t.finished = false; ComputeNextRun(t); }
            }
            break;
        }
    }
}

void Scheduler::ResetTask(int id) {
    std::scoped_lock lock(mutex_);
    for (auto& t : tasks_) {
        if (t.id == id) {
            t.runCount = 0;
            t.failCount = 0;
            t.lastRunTime = 0;
            t.finished = false;
            t.status = t.enabled ? TaskStatus::Waiting : TaskStatus::Disabled;
            t.history.clear();
            ComputeNextRun(t);
            break;
        }
    }
}

void Scheduler::RunTaskNow(int id) {
    std::scoped_lock lock(mutex_);
    pendingRunNow_.push_back(id);
}

std::vector<ScheduledTask> Scheduler::GetTasks() const {
    std::scoped_lock lock(mutex_);
    return tasks_;
}

void Scheduler::ClearTasks() {
    std::scoped_lock lock(mutex_);
    tasks_.clear();
}

int Scheduler::TaskCount() const {
    std::scoped_lock lock(mutex_);
    return (int)tasks_.size();
}

int Scheduler::ActiveTaskCount() const {
    std::scoped_lock lock(mutex_);
    int c = 0;
    for (const auto& t : tasks_) if (t.enabled && !t.finished) ++c;
    return c;
}

void Scheduler::ComputeNextRun(ScheduledTask& task) {
    const int64_t now = NowEpochSeconds();
    if (task.type == TaskType::OneShot) {
        if (task.triggerTime == 0 && !task.dateStr.empty() && !task.timeStr.empty())
            task.triggerTime = ParseDateTime(task.dateStr, task.timeStr);
        task.nextRunTime = task.triggerTime;
    } else {
        const int64_t periodSec = PeriodToSeconds(task.interval, task.unit);
        if (task.lastRunTime > 0)
            task.nextRunTime = task.lastRunTime + periodSec;
        else
            task.nextRunTime = now + task.startDelaySec + periodSec;
    }
}

bool Scheduler::IsInTimeWindow(const ScheduledTask& task) const {
    if (task.windowStartHour == 0 && task.windowEndHour == 0) return true;
    time_t now = (time_t)NowEpochSeconds();
    struct tm t{}; localtime_s(&t, &now);
    int h = t.tm_hour;
    if (task.windowStartHour <= task.windowEndHour)
        return h >= task.windowStartHour && h < task.windowEndHour;
    else // wraps midnight
        return h >= task.windowStartHour || h < task.windowEndHour;
}

void Scheduler::ThreadMain() {
    while (running_.load()) {
        Sleep(500);
        const int64_t now = NowEpochSeconds();

        std::vector<ScheduledTask> toRun;
        {
            std::scoped_lock lock(mutex_);

            // Handle "run now" requests
            for (int rid : pendingRunNow_) {
                for (auto& t : tasks_) {
                    if (t.id == rid) { toRun.push_back(t); break; }
                }
            }
            pendingRunNow_.clear();

            // Check scheduled triggers
            // Sort by priority (higher first)
            std::vector<int> indices;
            for (int i = 0; i < (int)tasks_.size(); ++i) indices.push_back(i);
            std::sort(indices.begin(), indices.end(), [&](int a, int b) {
                return tasks_[a].priority > tasks_[b].priority;
            });

            for (int idx : indices) {
                auto& t = tasks_[idx];
                if (!t.enabled || t.finished) continue;
                if (now < t.nextRunTime || t.nextRunTime <= 0) continue;
                if (!IsInTimeWindow(t)) continue;

                // Check if already in toRun (from RunNow)
                bool dup = false;
                for (const auto& r : toRun) if (r.id == t.id) { dup = true; break; }
                if (dup) continue;

                toRun.push_back(t);
                t.runCount++;
                t.lastRunTime = now;
                t.status = TaskStatus::Running;

                if (t.type == TaskType::OneShot) {
                    t.finished = true;
                    t.status = TaskStatus::Done;
                } else {
                    if (t.maxRuns > 0 && t.runCount >= t.maxRuns) {
                        t.finished = true;
                        t.status = TaskStatus::Done;
                    } else {
                        ComputeNextRun(t);
                    }
                }
            }
        }

        for (const auto& t : toRun) {
            LOG_INFO("Scheduler::ThreadMain", "Executing task id=%d name='%s' run#%d",
                t.id, t.name.c_str(), t.runCount);
            TaskRunRecord rec;
            rec.startTime = now;
            rec.success = true;
            if (callback_) {
                try {
                    callback_(t);
                } catch (const std::exception& ex) {
                    rec.success = false;
                    rec.errorMsg = ex.what();
                    LOG_ERROR("Scheduler::ThreadMain", "Task id=%d exception: %s", t.id, ex.what());
                } catch (...) {
                    rec.success = false;
                    rec.errorMsg = "unknown exception";
                    LOG_ERROR("Scheduler::ThreadMain", "Task id=%d unknown exception", t.id);
                }
            }
            rec.endTime = NowEpochSeconds();

            // Update history
            std::scoped_lock lock(mutex_);
            for (auto& mt : tasks_) {
                if (mt.id == t.id) {
                    mt.history.push_back(rec);
                    if ((int)mt.history.size() > 20) mt.history.erase(mt.history.begin());
                    if (!rec.success) {
                        mt.failCount++;
                        mt.status = TaskStatus::Failed;
                    } else if (!mt.finished) {
                        mt.status = TaskStatus::Waiting;
                    }
                    break;
                }
            }
        }
    }
}

int64_t Scheduler::NowEpochSeconds() {
    return (int64_t)std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t Scheduler::ParseDateTime(const std::string& date, const std::string& time) {
    struct tm t{};
    if (sscanf_s(date.c_str(), "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday) != 3) return 0;
    t.tm_year -= 1900; t.tm_mon -= 1;
    if (sscanf_s(time.c_str(), "%d:%d:%d", &t.tm_hour, &t.tm_min, &t.tm_sec) != 3) return 0;
    t.tm_isdst = -1;
    return (int64_t)mktime(&t);
}

std::string Scheduler::FormatEpoch(int64_t epoch) {
    if (epoch <= 0) return "-";
    time_t sec = (time_t)epoch;
    struct tm t{}; localtime_s(&t, &sec);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return buf;
}

std::string Scheduler::FormatDuration(int64_t seconds) {
    if (seconds < 0) return "-";
    if (seconds < 60) { char b[32]; snprintf(b, 32, "%llds", seconds); return b; }
    if (seconds < 3600) { char b[32]; snprintf(b, 32, "%lldm%llds", seconds / 60, seconds % 60); return b; }
    if (seconds < 86400) { char b[32]; snprintf(b, 32, "%lldh%lldm", seconds / 3600, (seconds % 3600) / 60); return b; }
    char b[32]; snprintf(b, 32, "%lldd%lldh", seconds / 86400, (seconds % 86400) / 3600); return b;
}

int64_t Scheduler::PeriodToSeconds(int interval, PeriodUnit unit) {
    switch (unit) {
    case PeriodUnit::Seconds: return interval;
    case PeriodUnit::Minutes: return interval * 60LL;
    case PeriodUnit::Hours:   return interval * 3600LL;
    case PeriodUnit::Days:    return interval * 86400LL;
    case PeriodUnit::Weeks:   return interval * 604800LL;
    default: return interval;
    }
}

const char* Scheduler::StatusName(TaskStatus s) {
    switch (s) {
    case TaskStatus::Idle:     return "空闲";
    case TaskStatus::Waiting:  return "等待中";
    case TaskStatus::Running:  return "执行中";
    case TaskStatus::Done:     return "已完成";
    case TaskStatus::Failed:   return "失败";
    case TaskStatus::Disabled: return "已禁用";
    default: return "?";
    }
}

std::string Scheduler::Serialize() const {
    std::scoped_lock lock(mutex_);
    std::ostringstream ss;
    for (const auto& t : tasks_) {
        ss << t.id << "|" << t.name << "|" << (int)t.type << "|"
           << t.dateStr << "|" << t.timeStr << "|"
           << t.interval << "|" << (int)t.unit << "|" << t.maxRuns << "|"
           << t.actionMode << "|" << t.actionPath << "|"
           << (t.enabled ? 1 : 0) << "|" << t.runCount << "|" << t.triggerTime << "|"
           << t.priority << "|" << t.startDelaySec << "|"
           << t.windowStartHour << "|" << t.windowEndHour << "|"
           << t.retryCount << "|" << t.retryDelaySec << "|"
           << t.actionSpeed << "|" << (t.actionBlockInput ? 1 : 0) << "|"
           << t.failCount << "|" << t.createdTime << "|"
           << t.description << "\n";
    }
    return ss.str();
}

void Scheduler::Deserialize(const std::string& data) {
    std::scoped_lock lock(mutex_);
    tasks_.clear();
    std::istringstream ss(data);
    std::string line;
    int maxId = 0;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        ScheduledTask t;
        std::istringstream ls(line);
        std::string field;
        int fi = 0;
        while (std::getline(ls, field, '|')) {
            switch (fi) {
            case 0: t.id = std::atoi(field.c_str()); break;
            case 1: t.name = field; break;
            case 2: t.type = (TaskType)std::atoi(field.c_str()); break;
            case 3: t.dateStr = field; break;
            case 4: t.timeStr = field; break;
            case 5: t.interval = std::atoi(field.c_str()); break;
            case 6: t.unit = (PeriodUnit)std::atoi(field.c_str()); break;
            case 7: t.maxRuns = std::atoi(field.c_str()); break;
            case 8: t.actionMode = std::atoi(field.c_str()); break;
            case 9: t.actionPath = field; break;
            case 10: t.enabled = (field == "1"); break;
            case 11: t.runCount = std::atoi(field.c_str()); break;
            case 12: t.triggerTime = std::atoll(field.c_str()); break;
            case 13: t.priority = std::atoi(field.c_str()); break;
            case 14: t.startDelaySec = std::atoi(field.c_str()); break;
            case 15: t.windowStartHour = std::atoi(field.c_str()); break;
            case 16: t.windowEndHour = std::atoi(field.c_str()); break;
            case 17: t.retryCount = std::atoi(field.c_str()); break;
            case 18: t.retryDelaySec = std::atoi(field.c_str()); break;
            case 19: t.actionSpeed = (float)std::atof(field.c_str()); break;
            case 20: t.actionBlockInput = (field == "1"); break;
            case 21: t.failCount = std::atoi(field.c_str()); break;
            case 22: t.createdTime = std::atoll(field.c_str()); break;
            case 23: t.description = field; break;
            }
            fi++;
        }
        if (t.id > 0) {
            t.finished = false;
            if (t.type == TaskType::OneShot && t.runCount > 0) t.finished = true;
            if (t.type == TaskType::Periodic && t.maxRuns > 0 && t.runCount >= t.maxRuns) t.finished = true;
            t.status = t.finished ? TaskStatus::Done : (t.enabled ? TaskStatus::Waiting : TaskStatus::Disabled);
            ComputeNextRun(t);
            tasks_.push_back(t);
            if (t.id >= maxId) maxId = t.id;
        }
    }
    nextId_ = maxId + 1;
}
