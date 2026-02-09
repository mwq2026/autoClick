#pragma once

#include <cstdint>

namespace trc {

static constexpr char kSignature[4] = { 'T', 'I', 'N', 'Y' };
static constexpr int32_t kVersion = 1;

struct FileHeader {
    char signature[4];
    int32_t version;
    int32_t totalEvents;
    int64_t totalDurationMicros;
};

#pragma pack(push, 1)
struct RawEvent {
    uint8_t type;
    int32_t x;
    int32_t y;
    int32_t data;
    int64_t timeDelta;
};
#pragma pack(pop)

enum class EventType : uint8_t {
    MouseMove = 1,
    MouseDown = 2,
    MouseUp = 3,
    KeyDown = 4,
    KeyUp = 5,
    Wheel = 6
};

} // namespace trc
