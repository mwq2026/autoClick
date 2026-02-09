#pragma once

#include <cstdint>
#include <thread>
#include <windows.h>

#include "core/HighResClock.h"

namespace timing {

inline void HighPrecisionWaitMicros(int64_t microseconds) {
    if (microseconds <= 0) return;
    const int64_t start = QpcNow();
    const int64_t freq = QpcFrequency();

    while (true) {
        const int64_t elapsedQpc = QpcNow() - start;
        const int64_t elapsedMicros = (elapsedQpc * 1'000'000LL) / freq;
        const int64_t remaining = microseconds - elapsedMicros;
        if (remaining <= 0) break;
        if (remaining > 2000) {
            Sleep(1);
        } else {
            std::this_thread::yield();
        }
    }
}

} // namespace timing
