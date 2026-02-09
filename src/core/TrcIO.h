#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/TrcFormat.h"

namespace trc {

struct TrcReadResult {
    FileHeader header{};
    std::vector<RawEvent> events;
};

bool WriteTrcFile(const std::wstring& filename, const std::vector<RawEvent>& events, int64_t* totalDurationMicrosOut);
bool ReadTrcFile(const std::wstring& filename, TrcReadResult* out);

} // namespace trc

