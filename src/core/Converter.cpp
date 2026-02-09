#include "core/Converter.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

#include <windows.h>

#include "core/Recorder.h"
#include "core/TrcFormat.h"

struct PathPoint {
    int x;
    int y;
    int64_t tMicros;
};

static double PerpDistance(const PathPoint& p, const PathPoint& a, const PathPoint& b) {
    const double x = static_cast<double>(p.x);
    const double y = static_cast<double>(p.y);
    const double x1 = static_cast<double>(a.x);
    const double y1 = static_cast<double>(a.y);
    const double x2 = static_cast<double>(b.x);
    const double y2 = static_cast<double>(b.y);
    const double dx = x2 - x1;
    const double dy = y2 - y1;
    const double denom = std::sqrt(dx * dx + dy * dy);
    if (denom < 1e-6) {
        const double ex = x - x1;
        const double ey = y - y1;
        return std::sqrt(ex * ex + ey * ey);
    }
    const double num = std::abs(dy * x - dx * y + x2 * y1 - y2 * x1);
    return num / denom;
}

static void RdpRecursive(const std::vector<PathPoint>& pts, int start, int end, double eps, std::set<int>& keep) {
    if (end <= start + 1) return;
    double maxD = 0.0;
    int idx = -1;
    for (int i = start + 1; i < end; ++i) {
        const double d = PerpDistance(pts[i], pts[start], pts[end]);
        if (d > maxD) {
            maxD = d;
            idx = i;
        }
    }
    if (idx >= 0 && maxD > eps) {
        keep.insert(idx);
        RdpRecursive(pts, start, idx, eps, keep);
        RdpRecursive(pts, idx, end, eps, keep);
    }
}

static std::vector<int> OptimizePath(const std::vector<PathPoint>& pts, double eps) {
    if (pts.size() <= 2) {
        std::vector<int> out;
        for (int i = 0; i < static_cast<int>(pts.size()); ++i) out.push_back(i);
        return out;
    }
    std::set<int> keep;
    keep.insert(0);
    keep.insert(static_cast<int>(pts.size() - 1));
    RdpRecursive(pts, 0, static_cast<int>(pts.size() - 1), eps, keep);

    std::vector<int> idx(keep.begin(), keep.end());
    std::sort(idx.begin(), idx.end());
    return idx;
}

static void WriteWaitUs(std::ofstream& out, int64_t* carryMicros, int64_t dtMicros) {
    dtMicros = std::max<int64_t>(0, dtMicros);
    const int64_t total = dtMicros + (carryMicros ? *carryMicros : 0);
    const int64_t us = total;
    if (carryMicros) *carryMicros = 0;
    if (us > 0) out << "wait_us(" << us << ")\n";
}

static void WriteEvent(std::ofstream& out, const trc::RawEvent& e) {
    const auto type = static_cast<trc::EventType>(e.type);
    if (type == trc::EventType::MouseMove) {
        out << "mouse_move(" << e.x << "," << e.y << ")\n";
        return;
    }
    if (type == trc::EventType::MouseDown) {
        out << "mouse_down(" << e.data << "," << e.x << "," << e.y << ")\n";
        return;
    }
    if (type == trc::EventType::MouseUp) {
        out << "mouse_up(" << e.data << "," << e.x << "," << e.y << ")\n";
        return;
    }
    if (type == trc::EventType::Wheel) {
        bool horizontal = (e.data & (1 << 30)) != 0;
        if ((static_cast<uint32_t>(e.data) & 0xFFFF0000u) == 0xFFFF0000u) horizontal = false;
        const int16_t delta16 = static_cast<int16_t>(e.data & 0xFFFF);
        const int delta = static_cast<int>(delta16);
        out << "mouse_wheel(" << delta << "," << e.x << "," << e.y << "," << (horizontal ? 1 : 0) << ")\n";
        return;
    }
    if (type == trc::EventType::KeyDown) {
        const bool ext = (e.data & LLKHF_EXTENDED) != 0;
        out << "vk_down(" << e.x << "," << (ext ? 1 : 0) << ")\n";
        return;
    }
    if (type == trc::EventType::KeyUp) {
        const bool ext = (e.data & LLKHF_EXTENDED) != 0;
        out << "vk_up(" << e.x << "," << (ext ? 1 : 0) << ")\n";
        return;
    }
}

bool Converter::TrcToLua(const std::wstring& trcFile, const std::wstring& luaFile, double tolerancePx) {
    Recorder rec;
    if (!rec.LoadFromFile(trcFile)) return false;

    std::vector<PathPoint> points;
    points.reserve(rec.Events().size());

    int64_t t = 0;
    for (const auto& e : rec.Events()) {
        t += e.timeDelta;
        if (static_cast<trc::EventType>(e.type) == trc::EventType::MouseMove) {
            points.push_back(PathPoint{ e.x, e.y, t });
        }
    }

    std::vector<int> keyIdx = OptimizePath(points, std::clamp(tolerancePx, 0.5, 20.0));

    std::ofstream out(std::filesystem::path(luaFile), std::ios::binary);
    if (!out) return false;

    out << "set_speed(1.0)\n";
    if (!points.empty()) {
        const auto& p0 = points[keyIdx.front()];
        out << "human_move(" << p0.x << "," << p0.y << ",1.0)\n";
        for (size_t i = 1; i < keyIdx.size(); ++i) {
            const auto& p = points[keyIdx[i]];
            const auto& prev = points[keyIdx[i - 1]];
            const int64_t dt = p.tMicros - prev.tMicros;
            const int64_t ms = std::max<int64_t>(0, dt / 1000);
            out << "human_move(" << p.x << "," << p.y << ",1.0)\n";
            if (ms > 0) out << "wait_ms(" << ms << ")\n";
        }
    }

    return out.good();
}

bool Converter::TrcToLuaFull(const std::wstring& trcFile, const std::wstring& luaFile) {
    Recorder rec;
    if (!rec.LoadFromFile(trcFile)) return false;

    std::ofstream out(std::filesystem::path(luaFile), std::ios::binary);
    if (!out) return false;

    out << "set_speed(1.0)\n";

    int64_t carryMicros = 0;
    for (const auto& e : rec.Events()) {
        WriteWaitUs(out, &carryMicros, e.timeDelta);
        WriteEvent(out, e);
    }

    return out.good();
}
