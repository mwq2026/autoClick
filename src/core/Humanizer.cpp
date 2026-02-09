#include "core/Humanizer.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <windows.h>

#include "core/HighPrecisionWait.h"

static LONG NormalizeAbsoluteX(int x) {
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    if (vw <= 1) return 0;
    const double t = (static_cast<double>(x - vx) / static_cast<double>(vw - 1));
    return static_cast<LONG>(std::clamp(t, 0.0, 1.0) * 65535.0);
}

static LONG NormalizeAbsoluteY(int y) {
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vh <= 1) return 0;
    const double t = (static_cast<double>(y - vy) / static_cast<double>(vh - 1));
    return static_cast<LONG>(std::clamp(t, 0.0, 1.0) * 65535.0);
}

static void SendMouseMoveAbs(int x, int y) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = NormalizeAbsoluteX(x);
    in.mi.dy = NormalizeAbsoluteY(y);
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &in, sizeof(in));
}

static void MoveCursorBestEffort(int x, int y) {
    if (SetCursorPos(x, y) != FALSE) return;
    SendMouseMoveAbs(x, y);
}

static DWORD MouseDownFlag(int button) {
    switch (button) {
    case 1: return MOUSEEVENTF_LEFTDOWN;
    case 2: return MOUSEEVENTF_RIGHTDOWN;
    case 3: return MOUSEEVENTF_MIDDLEDOWN;
    default: return 0;
    }
}

static DWORD MouseUpFlag(int button) {
    switch (button) {
    case 1: return MOUSEEVENTF_LEFTUP;
    case 2: return MOUSEEVENTF_RIGHTUP;
    case 3: return MOUSEEVENTF_MIDDLEUP;
    default: return 0;
    }
}

static void SendMouseButton(int button, bool down) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = down ? MouseDownFlag(button) : MouseUpFlag(button);
    if (in.mi.dwFlags == 0) return;
    SendInput(1, &in, sizeof(in));
}

static double EaseInOut(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

static POINT Bezier(const POINT& p0, const POINT& p1, const POINT& p2, const POINT& p3, double t) {
    const double u = 1.0 - t;
    const double tt = t * t;
    const double uu = u * u;
    const double uuu = uu * u;
    const double ttt = tt * t;

    const double x = (uuu * p0.x) + (3 * uu * t * p1.x) + (3 * u * tt * p2.x) + (ttt * p3.x);
    const double y = (uuu * p0.y) + (3 * uu * t * p1.y) + (3 * u * tt * p2.y) + (ttt * p3.y);
    POINT out{};
    out.x = static_cast<LONG>(std::llround(x));
    out.y = static_cast<LONG>(std::llround(y));
    return out;
}

namespace human {

void MoveTo(int x, int y, double speed) {
    POINT start{};
    GetCursorPos(&start);
    POINT end{ x, y };

    const double dx = static_cast<double>(end.x - start.x);
    const double dy = static_cast<double>(end.y - start.y);
    const double dist = std::sqrt(dx * dx + dy * dy);

    speed = std::clamp(speed, 0.1, 10.0);
    const double basePps = 2200.0 * speed;
    const double durationMs = std::clamp((dist / basePps) * 1000.0, 30.0, 1200.0);

    const int steps = static_cast<int>(std::clamp(dist / 8.0, 18.0, 140.0));
    const int64_t stepWait = static_cast<int64_t>((durationMs * 1000.0) / static_cast<double>(steps));

    static thread_local std::mt19937 rng{ std::random_device{}() };
    const double curve = std::clamp(dist * 0.18, 20.0, 180.0);
    std::uniform_real_distribution<double> off(-curve, curve);

    POINT c1{ static_cast<LONG>(start.x + dx * 0.25 + off(rng)), static_cast<LONG>(start.y + dy * 0.25 + off(rng)) };
    POINT c2{ static_cast<LONG>(start.x + dx * 0.75 + off(rng)), static_cast<LONG>(start.y + dy * 0.75 + off(rng)) };

    POINT last = start;
    for (int i = 1; i <= steps; ++i) {
        const double t = EaseInOut(static_cast<double>(i) / static_cast<double>(steps));
        const POINT p = Bezier(start, c1, c2, end, t);
        if (p.x != last.x || p.y != last.y) {
            MoveCursorBestEffort(p.x, p.y);
            last = p;
        }
        timing::HighPrecisionWaitMicros(stepWait);
    }
}

void Click(int button) {
    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> holdMs(50, 100);
    SendMouseButton(button, true);
    timing::HighPrecisionWaitMicros(static_cast<int64_t>(holdMs(rng)) * 1000);
    SendMouseButton(button, false);
}

void Scroll(int delta) {
    delta = std::clamp(delta, -2400, 2400);
    if (delta == 0) return;

    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> gapMs(12, 25);

    int remaining = delta;
    int step = delta;
    while (std::abs(step) > 0) {
        step = static_cast<int>(std::llround(static_cast<double>(step) * 0.6));
        if (step == 0) step = (remaining > 0) ? 120 : -120;
        if ((remaining > 0 && step > remaining) || (remaining < 0 && step < remaining)) step = remaining;

        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_WHEEL;
        in.mi.mouseData = static_cast<DWORD>(step);
        SendInput(1, &in, sizeof(in));

        remaining -= step;
        if (remaining == 0) break;
        timing::HighPrecisionWaitMicros(static_cast<int64_t>(gapMs(rng)) * 1000);
    }
}

} // namespace human
