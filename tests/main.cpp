#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <thread>

#include "core/Converter.h"
#include "core/Replayer.h"
#include "core/TrcIO.h"

static std::vector<trc::RawEvent> MakeEvents(size_t n) {
    std::mt19937 rng{ 12345 };
    std::uniform_int_distribution<int> xy(-800, 1600);
    std::uniform_int_distribution<int> dt(0, 20000);

    std::vector<trc::RawEvent> v;
    v.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        trc::RawEvent e{};
        e.type = static_cast<uint8_t>(trc::EventType::MouseMove);
        e.x = xy(rng);
        e.y = xy(rng);
        e.timeDelta = dt(rng);
        v.push_back(e);
    }
    return v;
}

static void TestTrcRoundTrip() {
    const auto temp = std::filesystem::temp_directory_path() / "acp_test.trc";
    auto events = MakeEvents(1000);

    int64_t totalWritten = 0;
    const bool wrote = trc::WriteTrcFile(temp.wstring(), events, &totalWritten);
    assert(wrote);
    assert(totalWritten >= 0);

    trc::TrcReadResult rr{};
    const bool read = trc::ReadTrcFile(temp.wstring(), &rr);
    assert(read);
    assert(rr.header.totalEvents == static_cast<int32_t>(events.size()));
    assert(rr.header.totalDurationMicros == totalWritten);
    assert(rr.events.size() == events.size());

    for (size_t i = 0; i < events.size(); ++i) {
        assert(rr.events[i].type == events[i].type);
        assert(rr.events[i].x == events[i].x);
        assert(rr.events[i].y == events[i].y);
        assert(rr.events[i].data == events[i].data);
        assert(rr.events[i].timeDelta == events[i].timeDelta);
    }
}

static void TestReplayerRestartNoTerminate() {
    Replayer r;
    r.SetDryRun(true);
    r.SetSpeed(10.0);

    auto events = MakeEvents(2000);
    assert(r.Start(events, false, 10.0));
    while (r.IsRunning()) std::this_thread::sleep_for(std::chrono::milliseconds(1));

    assert(r.Start(events, false, 10.0));
    while (r.IsRunning()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

static void TestReplayerStop() {
    Replayer r;
    r.SetDryRun(true);
    r.SetSpeed(0.5);

    auto events = MakeEvents(200000);
    assert(r.Start(events, false, 0.5));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    r.Stop();
    assert(!r.IsRunning());
}

static std::string ReadAllBytes(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void TestTrcToLuaFullIncludesWheelAndKey() {
    const auto trcPath = std::filesystem::temp_directory_path() / "acp_test_full.trc";
    const auto luaPath = std::filesystem::temp_directory_path() / "acp_test_full.lua";

    std::vector<trc::RawEvent> events;
    {
        trc::RawEvent e{};
        e.type = static_cast<uint8_t>(trc::EventType::Wheel);
        e.x = 100;
        e.y = 200;
        const int16_t delta = -120;
        e.data = static_cast<int32_t>(static_cast<uint16_t>(delta));
        e.timeDelta = 1234;
        events.push_back(e);
    }
    {
        trc::RawEvent e{};
        e.type = static_cast<uint8_t>(trc::EventType::KeyDown);
        e.x = 0x41;
        e.y = 0;
        e.data = 0;
        e.timeDelta = 2000;
        events.push_back(e);
    }
    {
        trc::RawEvent e{};
        e.type = static_cast<uint8_t>(trc::EventType::KeyUp);
        e.x = 0x41;
        e.y = 0;
        e.data = 0;
        e.timeDelta = 3000;
        events.push_back(e);
    }

    const bool wrote = trc::WriteTrcFile(trcPath.wstring(), events, nullptr);
    assert(wrote);

    const bool ok = Converter::TrcToLuaFull(trcPath.wstring(), luaPath.wstring());
    assert(ok);

    const std::string lua = ReadAllBytes(luaPath);
    assert(lua.find("mouse_wheel(-120,100,200,0)") != std::string::npos);
    assert(lua.find("vk_down(65,0)") != std::string::npos);
    assert(lua.find("vk_up(65,0)") != std::string::npos);
}

int main() {
    TestTrcRoundTrip();
    TestReplayerRestartNoTerminate();
    TestReplayerStop();
    TestTrcToLuaFullIncludesWheelAndKey();
    return 0;
}
