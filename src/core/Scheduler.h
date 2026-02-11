#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

enum class TaskType : int {
    OneShot  = 0,   // 指定日期触发
    Periodic = 1    // 周期执行
};

enum class PeriodUnit : int {
    Seconds = 0,
    Minutes = 1,
    Hours   = 2,
    Days    = 3,
    Weeks   = 4
};

enum class TaskStatus : int {
    Idle     = 0,
    Waiting  = 1,
    Running  = 2,
    Done     = 3,
    Failed   = 4,
    Disabled = 5
};

struct TaskRunRecord {
    int64_t     startTime{ 0 };
    int64_t     endTime{ 0 };
    bool        success{ true };
    std::string errorMsg;
};

struct ScheduledTask {
    int         id{ 0 };
    std::string name;
    std::string description;
    bool        enabled{ true };
    int         priority{ 0 };      // 0=normal, 1=high, 2=urgent
    TaskType    type{ TaskType::OneShot };

    // OneShot: target datetime (epoch seconds)
    int64_t     triggerTime{ 0 };
    std::string dateStr;    // YYYY-MM-DD
    std::string timeStr;    // HH:MM:SS

    // Periodic
    int         interval{ 60 };
    PeriodUnit  unit{ PeriodUnit::Seconds };
    int         maxRuns{ 0 };       // 0 = infinite
    int         startDelaySec{ 0 }; // delay before first run

    // Time window: only run between these hours (0 = no restriction)
    int         windowStartHour{ 0 };
    int         windowEndHour{ 0 };

    // Retry on failure
    int         retryCount{ 0 };    // 0 = no retry
    int         retryDelaySec{ 5 };

    // Action
    int         actionMode{ 0 };    // 0=trc replay, 1=lua script
    std::string actionPath;
    float       actionSpeed{ 1.0f };// replay speed (trc only)
    bool        actionBlockInput{ false };

    // Runtime state
    int         runCount{ 0 };
    int         failCount{ 0 };
    int64_t     lastRunTime{ 0 };
    int64_t     nextRunTime{ 0 };
    int64_t     createdTime{ 0 };
    bool        finished{ false };
    TaskStatus  status{ TaskStatus::Idle };

    // History (last N runs)
    std::vector<TaskRunRecord> history;
};

class Scheduler {
public:
    using ActionCallback = std::function<void(const ScheduledTask&)>;

    Scheduler();
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    void Start(ActionCallback callback);
    void Stop();
    bool IsRunning() const;

    int  AddTask(const ScheduledTask& task);
    void RemoveTask(int id);
    void UpdateTask(const ScheduledTask& task);
    void SetTaskEnabled(int id, bool enabled);
    void ResetTask(int id);
    void RunTaskNow(int id);

    std::vector<ScheduledTask> GetTasks() const;
    void ClearTasks();
    int  TaskCount() const;
    int  ActiveTaskCount() const;

    std::string Serialize() const;
    void Deserialize(const std::string& data);

    static int64_t NowEpochSeconds();
    static int64_t ParseDateTime(const std::string& date, const std::string& time);
    static std::string FormatEpoch(int64_t epoch);
    static std::string FormatDuration(int64_t seconds);
    static int64_t PeriodToSeconds(int interval, PeriodUnit unit);
    static const char* StatusName(TaskStatus s);

private:
    void ThreadMain();
    void ComputeNextRun(ScheduledTask& task);
    bool IsInTimeWindow(const ScheduledTask& task) const;

    mutable std::mutex mutex_;
    std::vector<ScheduledTask> tasks_;
    std::vector<int> pendingRunNow_;
    int nextId_{ 1 };

    std::atomic<bool> running_{ false };
    std::thread worker_;
    ActionCallback callback_;
};
