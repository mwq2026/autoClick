#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "core/TrcFormat.h"

class Replayer {
public:
    Replayer();
    ~Replayer();

    Replayer(const Replayer&) = delete;
    Replayer& operator=(const Replayer&) = delete;

    bool Start(std::vector<trc::RawEvent> events, bool blockInput, double speedFactor);
    void Stop();
    bool IsRunning() const;
    void Pause();
    void Resume();
    bool IsPaused() const;

    void SetDryRun(bool dryRun);
    int BlockInputState() const;

    void SetSpeed(double speedFactor);
    double Speed() const;

    float Progress01() const;

private:
    void ThreadMain(std::vector<trc::RawEvent> events, bool blockInput);
    void InjectEvent(const trc::RawEvent& e);

    std::atomic<bool> running_{ false };
    std::atomic<bool> stop_{ false };
    std::atomic<bool> paused_{ false };

    std::atomic<double> speedFactor_{ 1.0 };
    std::atomic<bool> dryRun_{ false };
    std::atomic<int> blockInputState_{ 0 };

    std::atomic<uint32_t> current_{ 0 };
    std::atomic<uint32_t> total_{ 0 };

    std::thread worker_;
};
