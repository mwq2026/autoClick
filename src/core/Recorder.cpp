#include "core/Recorder.h"

#include <algorithm>
#include <cstring>
#include <windows.h>

#include "core/Logger.h"
#include "core/TrcIO.h"

Recorder::Recorder() {
    ring_.resize(1u << 18);
}

Recorder::~Recorder() {
    Stop();
}

void Recorder::Start() {
    Clear();
    recording_.store(true, std::memory_order_release);
    StartDrainThread();
    LOG_INFO("Recorder::Start", "Recording started");
}

void Recorder::Stop() {
    recording_.store(false, std::memory_order_release);
    StopDrainThread();
    LOG_INFO("Recorder::Stop", "Recording stopped, total events=%zu", events_.size());
}

bool Recorder::IsRecording() const {
    return recording_.load(std::memory_order_acquire);
}

void Recorder::Clear() {
    {
        std::scoped_lock lock(eventsMutex_);
        events_.clear();
    }
    ringWrite_.store(0, std::memory_order_release);
    ringRead_.store(0, std::memory_order_release);
}

const std::vector<trc::RawEvent>& Recorder::Events() const {
    return events_;
}

int64_t Recorder::TotalDurationMicros() const {
    std::scoped_lock lock(eventsMutex_);
    int64_t total = 0;
    for (const auto& e : events_) total += e.timeDelta;
    return total;
}

bool Recorder::SaveToFile(const std::wstring& filename) const {
    std::vector<trc::RawEvent> copy;
    {
        std::scoped_lock lock(eventsMutex_);
        copy = events_;
    }
    bool ok = trc::WriteTrcFile(filename, copy, nullptr);
    if (ok) LOG_INFO("Recorder::SaveToFile", "Saved %zu events", copy.size());
    else LOG_ERROR("Recorder::SaveToFile", "Failed to save file");
    return ok;
}

bool Recorder::LoadFromFile(const std::wstring& filename) {
    trc::TrcReadResult rr{};
    if (!trc::ReadTrcFile(filename, &rr)) {
        LOG_ERROR("Recorder::LoadFromFile", "Failed to read trc file");
        return false;
    }

    {
        std::scoped_lock lock(eventsMutex_);
        events_ = std::move(rr.events);
    }
    ringWrite_.store(0, std::memory_order_release);
    ringRead_.store(0, std::memory_order_release);
    LOG_INFO("Recorder::LoadFromFile", "Loaded %zu events", events_.size());
    return true;
}

void Recorder::PushRawEvent(const trc::RawEvent& e) {
    if (!IsRecording()) return;

    const uint32_t size = static_cast<uint32_t>(ring_.size());
    const uint32_t write = ringWrite_.load(std::memory_order_relaxed);
    const uint32_t read = ringRead_.load(std::memory_order_acquire);
    if ((write - read) >= size) return;

    ring_[write % size] = e;
    ringWrite_.store(write + 1, std::memory_order_release);
}

void Recorder::StartDrainThread() {
    if (drainRunning_.exchange(true)) return;
    drainThread_ = std::thread([this] {
        std::vector<trc::RawEvent> local;
        local.reserve(4096);

        while (drainRunning_.load(std::memory_order_acquire)) {
            const uint32_t size = static_cast<uint32_t>(ring_.size());
            uint32_t read = ringRead_.load(std::memory_order_relaxed);
            const uint32_t write = ringWrite_.load(std::memory_order_acquire);

            local.clear();
            while (read != write && local.size() < 4096) {
                local.push_back(ring_[read % size]);
                ++read;
            }

            if (!local.empty()) {
                ringRead_.store(read, std::memory_order_release);
                std::scoped_lock lock(eventsMutex_);
                events_.insert(events_.end(), local.begin(), local.end());
            } else {
                Sleep(1);
            }
        }
    });
}

void Recorder::StopDrainThread() {
    if (!drainRunning_.exchange(false)) return;
    if (drainThread_.joinable()) drainThread_.join();
}
