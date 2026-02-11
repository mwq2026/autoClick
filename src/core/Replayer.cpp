#include "core/Replayer.h"

#include <algorithm>
#include <windows.h>

#include "core/HighPrecisionWait.h"
#include "core/Logger.h"

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

static bool WindowContainsPoint(HWND hwnd, const POINT& pt) {
    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) return false;
    return PtInRect(&rc, pt) != FALSE;
}

static HWND RootWindowAtSkipSelf(const POINT& pt) {
    HWND h = WindowFromPoint(pt);
    const DWORD selfPid = GetCurrentProcessId();
    for (int iter = 0; iter < 64 && h; ++iter) {
        HWND root = GetAncestor(h, GA_ROOT);
        if (!root) return nullptr;
        if (!WindowContainsPoint(root, pt)) {
            h = GetWindow(root, GW_HWNDNEXT);
            continue;
        }
        DWORD pid = 0;
        GetWindowThreadProcessId(root, &pid);
        if (pid != 0 && pid != selfPid) return root;
        h = GetWindow(root, GW_HWNDNEXT);
    }
    return nullptr;
}

static void FocusWindowAt(int x, int y) {
    POINT pt{ x, y };
    HWND hwnd = RootWindowAtSkipSelf(pt);
    if (!hwnd) return;

    HWND fg = GetForegroundWindow();
    const DWORD curTid = GetCurrentThreadId();
    const DWORD fgTid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    const DWORD targetTid = GetWindowThreadProcessId(hwnd, nullptr);

    bool attachedFg = false;
    bool attachedTarget = false;

    if (fgTid != curTid && fgTid != 0) {
        attachedFg = (AttachThreadInput(curTid, fgTid, TRUE) != 0);
    }
    if (targetTid != curTid && targetTid != 0 && targetTid != fgTid) {
        attachedTarget = (AttachThreadInput(curTid, targetTid, TRUE) != 0);
    }

    ShowWindow(hwnd, SW_SHOW);
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);

    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);

    if (attachedTarget) AttachThreadInput(curTid, targetTid, FALSE);
    if (attachedFg) AttachThreadInput(curTid, fgTid, FALSE);
    Sleep(10);
}

static DWORD MouseDownFlag(int button) {
    switch (button) {
    case 1: return MOUSEEVENTF_LEFTDOWN;
    case 2: return MOUSEEVENTF_RIGHTDOWN;
    case 3: return MOUSEEVENTF_MIDDLEDOWN;
    case 4: return MOUSEEVENTF_XDOWN;
    case 5: return MOUSEEVENTF_XDOWN;
    default: return 0;
    }
}

static DWORD MouseUpFlag(int button) {
    switch (button) {
    case 1: return MOUSEEVENTF_LEFTUP;
    case 2: return MOUSEEVENTF_RIGHTUP;
    case 3: return MOUSEEVENTF_MIDDLEUP;
    case 4: return MOUSEEVENTF_XUP;
    case 5: return MOUSEEVENTF_XUP;
    default: return 0;
    }
}

static DWORD MouseXButtonData(int button) {
    if (button == 4) return XBUTTON1;
    if (button == 5) return XBUTTON2;
    return 0;
}

Replayer::Replayer() = default;

Replayer::~Replayer() {
    Stop();
}

bool Replayer::Start(std::vector<trc::RawEvent> events, bool blockInput, double speedFactor) {
    if (running_.load(std::memory_order_acquire)) return false;
    if (events.empty()) return false;

    if (worker_.joinable()) worker_.join();
    blockInputState_.store(0, std::memory_order_release);

    speedFactor = std::clamp(speedFactor, 0.5, 10.0);
    speedFactor_.store(speedFactor, std::memory_order_release);

    stop_.store(false, std::memory_order_release);
    paused_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    current_.store(0, std::memory_order_release);
    total_.store(static_cast<uint32_t>(events.size()), std::memory_order_release);

    LOG_INFO("Replayer::Start", "Replay starting: %zu events, speed=%.1f, blockInput=%d",
        events.size(), speedFactor, blockInput ? 1 : 0);

    worker_ = std::thread([this, ev = std::move(events), blockInput]() mutable {
        ThreadMain(std::move(ev), blockInput);
    });
    return true;
}

void Replayer::Stop() {
    LOG_INFO("Replayer::Stop", "Replay stop requested");
    stop_.store(true, std::memory_order_release);
    if (worker_.joinable()) worker_.join();
    running_.store(false, std::memory_order_release);
}

bool Replayer::IsRunning() const {
    return running_.load(std::memory_order_acquire);
}

void Replayer::Pause() {
    paused_.store(true, std::memory_order_release);
    LOG_INFO("Replayer::Pause", "Replay paused");
}

void Replayer::Resume() {
    paused_.store(false, std::memory_order_release);
    LOG_INFO("Replayer::Resume", "Replay resumed");
}

bool Replayer::IsPaused() const {
    return paused_.load(std::memory_order_acquire);
}

void Replayer::SetDryRun(bool dryRun) {
    dryRun_.store(dryRun, std::memory_order_release);
}

int Replayer::BlockInputState() const {
    return blockInputState_.load(std::memory_order_acquire);
}

void Replayer::SetSpeed(double speedFactor) {
    speedFactor_.store(std::clamp(speedFactor, 0.5, 10.0), std::memory_order_release);
}

double Replayer::Speed() const {
    return speedFactor_.load(std::memory_order_acquire);
}

float Replayer::Progress01() const {
    const uint32_t total = total_.load(std::memory_order_acquire);
    if (total == 0) return 0.0f;
    const uint32_t cur = current_.load(std::memory_order_acquire);
    return std::clamp(static_cast<float>(cur) / static_cast<float>(total), 0.0f, 1.0f);
}

void Replayer::ThreadMain(std::vector<trc::RawEvent> events, bool blockInput) {
    const BOOL canBlock = blockInput ? BlockInput(TRUE) : TRUE;
    const bool blocked = (blockInput && canBlock == TRUE);
    if (blockInput) {
        blockInputState_.store(blocked ? 1 : -1, std::memory_order_release);
        if (blocked) LOG_INFO("Replayer::ThreadMain", "BlockInput enabled");
        else LOG_WARN("Replayer::ThreadMain", "BlockInput failed (may need admin)");
    }

    const bool dryRun = dryRun_.load(std::memory_order_acquire);
    for (uint32_t i = 0; i < events.size(); ++i) {
        if (stop_.load(std::memory_order_acquire)) break;

        // Wait while paused
        while (paused_.load(std::memory_order_acquire)) {
            if (stop_.load(std::memory_order_acquire)) break;
            Sleep(50);
        }
        if (stop_.load(std::memory_order_acquire)) break;

        const double speed = speedFactor_.load(std::memory_order_acquire);
        const int64_t waitMicros = static_cast<int64_t>(static_cast<double>(events[i].timeDelta) / speed);
        timing::HighPrecisionWaitMicros(waitMicros);

        if (!dryRun) InjectEvent(events[i]);
        current_.store(i + 1, std::memory_order_release);
    }

    if (blocked) BlockInput(FALSE);
    if (blocked) blockInputState_.store(0, std::memory_order_release);
    running_.store(false, std::memory_order_release);
    LOG_INFO("Replayer::ThreadMain", "Replay finished, played %u/%zu events",
        current_.load(), events.size());
}

void Replayer::InjectEvent(const trc::RawEvent& e) {
    const auto type = static_cast<trc::EventType>(e.type);

    if (type == trc::EventType::MouseMove) {
        MoveCursorBestEffort(e.x, e.y);
        return;
    }

    if (type == trc::EventType::MouseDown || type == trc::EventType::MouseUp) {
        MoveCursorBestEffort(e.x, e.y);
        const int button = e.data;
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = (type == trc::EventType::MouseDown) ? MouseDownFlag(button) : MouseUpFlag(button);
        if (in.mi.dwFlags == 0) return;
        if (button == 4 || button == 5) in.mi.mouseData = MouseXButtonData(button);
        SendInput(1, &in, sizeof(in));
        return;
    }

    if (type == trc::EventType::Wheel) {
        MoveCursorBestEffort(e.x, e.y);
        FocusWindowAt(e.x, e.y);
        bool horizontal = (e.data & (1 << 30)) != 0;
        if ((static_cast<uint32_t>(e.data) & 0xFFFF0000u) == 0xFFFF0000u) horizontal = false;
        const int16_t delta16 = static_cast<int16_t>(e.data & 0xFFFF);
        const int delta = static_cast<int>(delta16);
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = horizontal ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL;
        in.mi.mouseData = static_cast<DWORD>(delta);
        SendInput(1, &in, sizeof(in));
        return;
    }

    if (type == trc::EventType::KeyDown || type == trc::EventType::KeyUp) {
        POINT pt{};
        if (GetCursorPos(&pt)) FocusWindowAt(pt.x, pt.y);
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        const WORD vk = static_cast<WORD>(e.x);
        const WORD sc = static_cast<WORD>(e.y);
        in.ki.wVk = (sc != 0) ? 0 : vk;
        in.ki.wScan = sc;
        in.ki.dwFlags = (sc != 0) ? KEYEVENTF_SCANCODE : 0;
        if ((e.data & LLKHF_EXTENDED) != 0) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        if (type == trc::EventType::KeyUp) in.ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(in));
        return;
    }
}
