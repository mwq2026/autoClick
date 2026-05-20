#pragma once
// InputUtils.h — shared low-level input helpers used by Replayer, LuaEngine, and Humanizer.
// Extracted to eliminate code duplication across those translation units.

#include <algorithm>
#include <windows.h>

namespace input {

inline LONG NormalizeAbsoluteX(int x) {
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    if (vw <= 1) return 0;
    const double t = (static_cast<double>(x - vx) / static_cast<double>(vw - 1));
    return static_cast<LONG>(std::clamp(t, 0.0, 1.0) * 65535.0);
}

inline LONG NormalizeAbsoluteY(int y) {
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vh <= 1) return 0;
    const double t = (static_cast<double>(y - vy) / static_cast<double>(vh - 1));
    return static_cast<LONG>(std::clamp(t, 0.0, 1.0) * 65535.0);
}

inline void SendMouseMoveAbs(int x, int y) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = NormalizeAbsoluteX(x);
    in.mi.dy = NormalizeAbsoluteY(y);
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &in, sizeof(in));
}

inline void MoveCursorBestEffort(int x, int y) {
    if (SetCursorPos(x, y) != FALSE) return;
    SendMouseMoveAbs(x, y);
}

inline DWORD MouseDownFlag(int button) {
    switch (button) {
    case 1: return MOUSEEVENTF_LEFTDOWN;
    case 2: return MOUSEEVENTF_RIGHTDOWN;
    case 3: return MOUSEEVENTF_MIDDLEDOWN;
    case 4: return MOUSEEVENTF_XDOWN;
    case 5: return MOUSEEVENTF_XDOWN;
    default: return 0;
    }
}

inline DWORD MouseUpFlag(int button) {
    switch (button) {
    case 1: return MOUSEEVENTF_LEFTUP;
    case 2: return MOUSEEVENTF_RIGHTUP;
    case 3: return MOUSEEVENTF_MIDDLEUP;
    case 4: return MOUSEEVENTF_XUP;
    case 5: return MOUSEEVENTF_XUP;
    default: return 0;
    }
}

inline DWORD MouseXButtonData(int button) {
    if (button == 4) return XBUTTON1;
    if (button == 5) return XBUTTON2;
    return 0;
}

// Focus the root window at (x, y), skipping our own process.
// Returns true if a target window was found and activated.
inline bool FocusWindowAt(int x, int y) {
    POINT pt{ x, y };
    // Find root window at point, skipping our own process
    HWND h = WindowFromPoint(pt);
    const DWORD selfPid = GetCurrentProcessId();
    HWND hwnd = nullptr;
    for (int iter = 0; iter < 64 && h; ++iter) {
        HWND root = GetAncestor(h, GA_ROOT);
        if (!root) break;
        RECT rc{};
        if (GetWindowRect(root, &rc) && PtInRect(&rc, pt)) {
            DWORD pid = 0;
            GetWindowThreadProcessId(root, &pid);
            if (pid != 0 && pid != selfPid) { hwnd = root; break; }
        }
        h = GetWindow(root, GW_HWNDNEXT);
    }
    if (!hwnd) return false;

    HWND fg = GetForegroundWindow();
    const DWORD curTid = GetCurrentThreadId();
    const DWORD fgTid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    const DWORD targetTid = GetWindowThreadProcessId(hwnd, nullptr);

    bool attachedFg = false, attachedTarget = false;
    if (fgTid != curTid && fgTid != 0)
        attachedFg = (AttachThreadInput(curTid, fgTid, TRUE) != 0);
    if (targetTid != curTid && targetTid != 0 && targetTid != fgTid)
        attachedTarget = (AttachThreadInput(curTid, targetTid, TRUE) != 0);

    ShowWindow(hwnd, SW_SHOW);
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);

    if (attachedTarget) AttachThreadInput(curTid, targetTid, FALSE);
    if (attachedFg)     AttachThreadInput(curTid, fgTid,    FALSE);
    Sleep(10);
    return true;
}

} // namespace input
