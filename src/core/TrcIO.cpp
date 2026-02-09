#include "core/TrcIO.h"

#include <cstring>
#include <fstream>

namespace trc {

bool WriteTrcFile(const std::wstring& filename, const std::vector<RawEvent>& events, int64_t* totalDurationMicrosOut) {
    int64_t total = 0;
    for (const auto& e : events) total += e.timeDelta;
    if (totalDurationMicrosOut) *totalDurationMicrosOut = total;

    FileHeader hdr{};
    std::memcpy(hdr.signature, kSignature, sizeof(hdr.signature));
    hdr.version = kVersion;
    hdr.totalEvents = static_cast<int32_t>(events.size());
    hdr.totalDurationMicros = total;

    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (!events.empty()) {
        out.write(reinterpret_cast<const char*>(events.data()), static_cast<std::streamsize>(events.size() * sizeof(RawEvent)));
    }
    return out.good();
}

bool ReadTrcFile(const std::wstring& filename, TrcReadResult* out) {
    if (!out) return false;
    std::ifstream in(filename, std::ios::binary);
    if (!in) return false;

    FileHeader hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) return false;
    if (std::memcmp(hdr.signature, kSignature, sizeof(hdr.signature)) != 0) return false;
    if (hdr.version != kVersion) return false;
    if (hdr.totalEvents < 0) return false;

    std::vector<RawEvent> events;
    events.resize(static_cast<size_t>(hdr.totalEvents));
    if (!events.empty()) {
        in.read(reinterpret_cast<char*>(events.data()), static_cast<std::streamsize>(events.size() * sizeof(RawEvent)));
        if (!in) return false;
    }

    out->header = hdr;
    out->events = std::move(events);
    return true;
}

} // namespace trc

