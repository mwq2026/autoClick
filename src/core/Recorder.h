#pragma once

#include <atomic>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/TrcFormat.h"

class Recorder {
public:
    Recorder();
    ~Recorder();

    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    void Start();
    void Stop();
    bool IsRecording() const;

    void Clear();
    const std::vector<trc::RawEvent>& Events() const;
    int64_t TotalDurationMicros() const;

    bool SaveToFile(const std::wstring& filename) const;
    bool LoadFromFile(const std::wstring& filename);

    void PushRawEvent(const trc::RawEvent& e);

private:
    void StartDrainThread();
    void StopDrainThread();

    std::atomic<bool> recording_{ false };

    std::vector<trc::RawEvent> events_;
    mutable std::mutex eventsMutex_;

    std::vector<trc::RawEvent> ring_;
    std::atomic<uint32_t> ringWrite_{ 0 };
    std::atomic<uint32_t> ringRead_{ 0 };

    std::atomic<bool> drainRunning_{ false };
    std::thread drainThread_;
};
