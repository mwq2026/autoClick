#pragma once

#include <cstdint>
#include <windows.h>

namespace timing {

inline int64_t QpcFrequency() {
    static int64_t freq = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return static_cast<int64_t>(f.QuadPart);
    }();
    return freq;
}

inline int64_t QpcNow() {
    LARGE_INTEGER v{};
    QueryPerformanceCounter(&v);
    return static_cast<int64_t>(v.QuadPart);
}

inline int64_t QpcDeltaToMicros(int64_t qpcDelta) {
    const int64_t freq = QpcFrequency();
    return (qpcDelta * 1'000'000LL) / freq;
}

inline int64_t MicrosNow() {
    return QpcDeltaToMicros(QpcNow());
}

} // namespace timing
