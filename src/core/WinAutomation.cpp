#include "core/WinAutomation.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cwctype>

namespace winauto {

static bool WindowContainsPoint(HWND hwnd, const POINT& pt) {
    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) return false;
    return PtInRect(&rc, pt) != FALSE;
}

HWND RootWindowAtSkipSelf(const POINT& pt) {
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

HWND WindowFromPointSkipSelf(const POINT& pt) {
    HWND h = WindowFromPoint(pt);
    if (!h) return nullptr;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != 0 && pid == GetCurrentProcessId()) return nullptr;
    return h;
}

bool ActivateWindow(HWND hwnd) {
    if (!hwnd) return false;
    hwnd = GetAncestor(hwnd, GA_ROOT);
    if (!hwnd) return false;
    if (IsWindow(hwnd) == FALSE) return false;

    DWORD targetPid = 0;
    GetWindowThreadProcessId(hwnd, &targetPid);
    if (targetPid == GetCurrentProcessId()) return false;

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
    return true;
}

std::wstring WindowTitle(HWND hwnd) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return {};
    const int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return {};
    std::wstring out;
    out.resize(static_cast<size_t>(len) + 1);
    const int got = GetWindowTextW(hwnd, out.data(), len + 1);
    if (got <= 0) return {};
    out.resize(static_cast<size_t>(got));
    return out;
}

std::wstring WindowClass(HWND hwnd) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return {};
    wchar_t buf[256]{};
    const int got = GetClassNameW(hwnd, buf, static_cast<int>(std::size(buf)));
    if (got <= 0) return {};
    return std::wstring(buf, buf + got);
}

uint32_t WindowPid(HWND hwnd) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return static_cast<uint32_t>(pid);
}

bool WindowRect(HWND hwnd, RECT* out) {
    if (!out) return false;
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    return GetWindowRect(hwnd, out) != FALSE;
}

bool WindowClientSize(HWND hwnd, int* wOut, int* hOut) {
    if (!wOut || !hOut) return false;
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    RECT rc{};
    if (GetClientRect(hwnd, &rc) == FALSE) return false;
    *wOut = rc.right - rc.left;
    *hOut = rc.bottom - rc.top;
    return true;
}

static bool SetWindowPosSimple(HWND hwnd, HWND insertAfter, uint32_t flags) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    return SetWindowPos(hwnd, insertAfter, 0, 0, 0, 0, flags) != FALSE;
}

bool WindowSetTopmost(HWND hwnd, bool on) {
    const uint32_t flags = SWP_NOMOVE | SWP_NOSIZE;
    return SetWindowPosSimple(hwnd, on ? HWND_TOPMOST : HWND_NOTOPMOST, flags);
}

bool WindowBringToTop(HWND hwnd) {
    const uint32_t flags = SWP_NOMOVE | SWP_NOSIZE;
    return SetWindowPosSimple(hwnd, HWND_TOP, flags);
}

bool WindowSendToBack(HWND hwnd) {
    const uint32_t flags = SWP_NOMOVE | SWP_NOSIZE;
    return SetWindowPosSimple(hwnd, HWND_BOTTOM, flags);
}

bool WindowShow(HWND hwnd) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    return ShowWindow(hwnd, SW_SHOW) != FALSE;
}

bool WindowHide(HWND hwnd) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    return ShowWindow(hwnd, SW_HIDE) != FALSE;
}

bool WindowMinimize(HWND hwnd) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    return ShowWindow(hwnd, SW_MINIMIZE) != FALSE;
}

bool WindowMaximize(HWND hwnd) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    return ShowWindow(hwnd, SW_MAXIMIZE) != FALSE;
}

bool WindowRestore(HWND hwnd) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    return ShowWindow(hwnd, SW_RESTORE) != FALSE;
}

bool WindowMove(HWND hwnd, int x, int y) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    const uint32_t flags = SWP_NOSIZE | SWP_NOZORDER;
    return SetWindowPos(hwnd, nullptr, x, y, 0, 0, flags) != FALSE;
}

bool WindowResize(HWND hwnd, int w, int h) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    const uint32_t flags = SWP_NOMOVE | SWP_NOZORDER;
    return SetWindowPos(hwnd, nullptr, 0, 0, w, h, flags) != FALSE;
}

bool WindowSetRect(HWND hwnd, int x, int y, int w, int h) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    const uint32_t flags = SWP_NOZORDER;
    return SetWindowPos(hwnd, nullptr, x, y, w, h, flags) != FALSE;
}

bool WindowClose(HWND hwnd) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;
    return PostMessageW(hwnd, WM_CLOSE, 0, 0) != FALSE;
}

bool WindowCloseForce(HWND hwnd, uint32_t waitMs) {
    if (!hwnd || IsWindow(hwnd) == FALSE) return false;

    DWORD_PTR ignored = 0;
    SendMessageTimeoutW(hwnd, WM_CLOSE, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, std::max<uint32_t>(1, waitMs), &ignored);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (IsWindow(hwnd) == FALSE) return true;
        Sleep(10);
    }
    if (IsWindow(hwnd) == FALSE) return true;

    const DWORD pid = WindowPid(hwnd);
    if (pid == 0 || pid == GetCurrentProcessId()) return false;
    HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!h) return false;
    const BOOL ok = TerminateProcess(h, 1);
    CloseHandle(h);
    return ok != FALSE;
}

struct FindCtx {
    std::wstring titleSubstr;
    std::wstring className;
    bool visibleOnly{ true };
    bool skipSelf{ true };
    DWORD selfPid{ 0 };
    std::vector<HWND>* out{ nullptr };
};

static bool ContainsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](wchar_t a, wchar_t b) { return ::towlower(a) == ::towlower(b); }
    );
    return it != haystack.end();
}

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<FindCtx*>(lParam);
    if (!ctx || !ctx->out) return TRUE;
    if (IsWindow(hwnd) == FALSE) return TRUE;

    if (ctx->visibleOnly && IsWindowVisible(hwnd) == FALSE) return TRUE;

    if (ctx->skipSelf) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != 0 && pid == ctx->selfPid) return TRUE;
    }

    const std::wstring title = WindowTitle(hwnd);
    if (!ContainsCaseInsensitive(title, ctx->titleSubstr)) return TRUE;

    if (!ctx->className.empty()) {
        const std::wstring cls = WindowClass(hwnd);
        if (!ContainsCaseInsensitive(cls, ctx->className)) return TRUE;
    }

    ctx->out->push_back(hwnd);
    return TRUE;
}

std::vector<HWND> FindWindowsByTitleContains(const std::wstring& titleSubstr, const std::wstring& className, bool visibleOnly, bool skipSelf) {
    std::vector<HWND> out;
    FindCtx ctx;
    ctx.titleSubstr = titleSubstr;
    ctx.className = className;
    ctx.visibleOnly = visibleOnly;
    ctx.skipSelf = skipSelf;
    ctx.selfPid = GetCurrentProcessId();
    ctx.out = &out;
    EnumWindows(&EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    return out;
}

uint32_t ProcessStart(const std::wstring& path, const std::wstring& args, const std::wstring& cwd) {
    if (path.empty()) return 0;

    std::wstring cmd;
    cmd.reserve(path.size() + args.size() + 4);
    cmd.append(L"\"");
    cmd.append(path);
    cmd.append(L"\"");
    if (!args.empty()) {
        cmd.push_back(L' ');
        cmd.append(args);
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::wstring cmdMutable = cmd;
    const BOOL ok = CreateProcessW(
        nullptr,
        cmdMutable.empty() ? nullptr : cmdMutable.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        cwd.empty() ? nullptr : cwd.c_str(),
        &si,
        &pi
    );
    if (!ok) return 0;

    const uint32_t pid = static_cast<uint32_t>(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pid;
}

bool ProcessIsRunning(uint32_t pid) {
    if (pid == 0) return false;
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h) return false;
    const DWORD r = WaitForSingleObject(h, 0);
    CloseHandle(h);
    return r == WAIT_TIMEOUT;
}

bool ProcessWait(uint32_t pid, uint32_t timeoutMs) {
    if (pid == 0) return false;
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h) return false;
    const DWORD r = WaitForSingleObject(h, timeoutMs);
    CloseHandle(h);
    return r == WAIT_OBJECT_0;
}

bool ProcessKill(uint32_t pid, uint32_t exitCode) {
    if (pid == 0) return false;
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;
    const BOOL ok = TerminateProcess(h, exitCode);
    CloseHandle(h);
    return ok != FALSE;
}

bool ClipboardSetText(const std::wstring& text) {
    if (OpenClipboard(nullptr) == FALSE) return false;
    EmptyClipboard();

    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!h) {
        CloseClipboard();
        return false;
    }
    void* p = GlobalLock(h);
    if (!p) {
        GlobalFree(h);
        CloseClipboard();
        return false;
    }
    std::memcpy(p, text.c_str(), bytes);
    GlobalUnlock(h);

    if (SetClipboardData(CF_UNICODETEXT, h) == nullptr) {
        GlobalFree(h);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

std::wstring ClipboardGetText() {
    if (OpenClipboard(nullptr) == FALSE) return {};
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (!h) {
        CloseClipboard();
        return {};
    }
    const wchar_t* p = static_cast<const wchar_t*>(GlobalLock(h));
    if (!p) {
        CloseClipboard();
        return {};
    }
    std::wstring out = p;
    GlobalUnlock(h);
    CloseClipboard();
    return out;
}

bool CursorPos(POINT* out) {
    if (!out) return false;
    return GetCursorPos(out) != FALSE;
}

bool CursorSet(int x, int y) {
    return SetCursorPos(x, y) != FALSE;
}

bool ScreenSize(int* wOut, int* hOut) {
    if (!wOut || !hOut) return false;
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (w <= 0 || h <= 0) return false;
    *wOut = w;
    *hOut = h;
    return true;
}

bool PixelGet(int x, int y, uint8_t* rOut, uint8_t* gOut, uint8_t* bOut) {
    if (!rOut || !gOut || !bOut) return false;
    HDC dc = GetDC(nullptr);
    if (!dc) return false;
    const COLORREF c = GetPixel(dc, x, y);
    ReleaseDC(nullptr, dc);
    if (c == CLR_INVALID) return false;
    *rOut = static_cast<uint8_t>(GetRValue(c));
    *gOut = static_cast<uint8_t>(GetGValue(c));
    *bOut = static_cast<uint8_t>(GetBValue(c));
    return true;
}

} // namespace winauto
