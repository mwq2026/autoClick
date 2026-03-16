#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <thread>

#include "core/Converter.h"
#include "core/Replayer.h"
#include "core/Scheduler.h"
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

static void TestTrcReadRejectHugeEventCount() {
    const auto temp = std::filesystem::temp_directory_path() / "acp_test_huge.trc";

    // Write a header with an absurdly large event count
    trc::FileHeader hdr{};
    std::memcpy(hdr.signature, trc::kSignature, sizeof(hdr.signature));
    hdr.version = trc::kVersion;
    hdr.totalEvents = 60'000'000; // above 50M limit
    hdr.totalDurationMicros = 0;

    {
        std::ofstream out(temp, std::ios::binary);
        assert(out.good());
        out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    }

    trc::TrcReadResult rr{};
    const bool read = trc::ReadTrcFile(temp.wstring(), &rr);
    assert(!read); // Should be rejected
}

static void TestSchedulerSerializeRoundTrip() {
    Scheduler sched;

    ScheduledTask t1;
    t1.name = "simple task";
    t1.type = TaskType::Periodic;
    t1.interval = 60;
    t1.unit = PeriodUnit::Seconds;
    t1.actionMode = 0;
    t1.actionPath = "C:\\Users\\test\\file.trc";
    t1.description = "a normal description";
    t1.enabled = true;
    sched.AddTask(t1);

    ScheduledTask t2;
    t2.name = "task|with|pipes";
    t2.type = TaskType::OneShot;
    t2.dateStr = "2026-01-01";
    t2.timeStr = "12:00:00";
    t2.actionMode = 1;
    t2.actionPath = "C:\\path|dir\\script.lua";
    t2.description = "line1\nline2|pipes\\backslash";
    t2.enabled = false;
    sched.AddTask(t2);

    const std::string data = sched.Serialize();

    Scheduler sched2;
    sched2.Deserialize(data);

    auto tasks = sched2.GetTasks();
    assert(tasks.size() == 2);

    assert(tasks[0].name == "simple task");
    assert(tasks[0].actionPath == "C:\\Users\\test\\file.trc");
    assert(tasks[0].description == "a normal description");

    assert(tasks[1].name == "task|with|pipes");
    assert(tasks[1].actionPath == "C:\\path|dir\\script.lua");
    assert(tasks[1].description == "line1\nline2|pipes\\backslash");
}

static void TestScrollAlgorithmTerminates() {
    // Test the Humanizer::Scroll algorithm logic without Windows APIs
    auto simulate = [](int delta) {
        delta = std::max(-2400, std::min(delta, 2400));
        if (delta == 0) return 0;

        int remaining = delta;
        int step = delta;
        int iterations = 0;
        int total = 0;

        while (remaining != 0) {
            step = static_cast<int>(std::llround(static_cast<double>(step) * 0.6));
            if (step == 0) step = (remaining > 0) ? 120 : -120;
            if ((remaining > 0 && step > remaining) || (remaining < 0 && step < remaining)) step = remaining;

            total += step;
            remaining -= step;
            iterations++;
            assert(iterations < 10000); // Must terminate
            if (remaining == 0) break;
        }
        return total;
    };

    // All steps must sum to the original delta
    int deltas[] = { 1, -1, 2, -2, 50, -50, 120, -120, 240, -240, 1000, -1000, 2400, -2400 };
    for (int d : deltas) {
        assert(simulate(d) == d);
    }
    assert(simulate(0) == 0);
}

int main() {
    TestTrcRoundTrip();
    TestReplayerRestartNoTerminate();
    TestReplayerStop();
    TestTrcToLuaFullIncludesWheelAndKey();
    TestTrcReadRejectHugeEventCount();
    TestSchedulerSerializeRoundTrip();
    TestScrollAlgorithmTerminates();
    return 0;
}
