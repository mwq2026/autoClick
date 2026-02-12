#include "core/WinAutomation.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cwctype>

#include <commctrl.h>

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

// ─── Spy++ / UI Automation extensions ───────────────────────────────────────

HWND WindowParent(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return nullptr;
    return GetParent(hwnd);
}

HWND WindowOwner(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return nullptr;
    return GetWindow(hwnd, GW_OWNER);
}

HWND WindowChild(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return nullptr;
    return GetWindow(hwnd, GW_CHILD);
}

HWND WindowNextSibling(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return nullptr;
    return GetWindow(hwnd, GW_HWNDNEXT);
}

HWND WindowPrevSibling(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return nullptr;
    return GetWindow(hwnd, GW_HWNDPREV);
}

static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {
    auto* vec = reinterpret_cast<std::vector<HWND>*>(lParam);
    if (vec) vec->push_back(hwnd);
    return TRUE;
}

std::vector<HWND> WindowChildren(HWND hwnd, bool recursive) {
    std::vector<HWND> out;
    if (!hwnd || !IsWindow(hwnd)) return out;
    if (recursive) {
        EnumChildWindows(hwnd, EnumChildProc, reinterpret_cast<LPARAM>(&out));
    } else {
        HWND child = GetWindow(hwnd, GW_CHILD);
        while (child) {
            out.push_back(child);
            child = GetWindow(child, GW_HWNDNEXT);
        }
    }
    return out;
}

HWND WindowDesktop() { return GetDesktopWindow(); }

uint32_t WindowStyle(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (uint32_t)GetWindowLongW(hwnd, GWL_STYLE);
}

uint32_t WindowExStyle(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (uint32_t)GetWindowLongW(hwnd, GWL_EXSTYLE);
}

bool WindowSetStyle(HWND hwnd, uint32_t style) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    SetWindowLongW(hwnd, GWL_STYLE, (LONG)style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    return true;
}

bool WindowSetExStyle(HWND hwnd, uint32_t exStyle) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    SetWindowLongW(hwnd, GWL_EXSTYLE, (LONG)exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    return true;
}

bool WindowIsVisible(HWND hwnd) { return hwnd && IsWindow(hwnd) && ::IsWindowVisible(hwnd); }
bool WindowIsEnabled(HWND hwnd) { return hwnd && IsWindow(hwnd) && ::IsWindowEnabled(hwnd); }
bool WindowIsFocused(HWND hwnd) { return hwnd && GetFocus() == hwnd; }
bool WindowIsMinimized(HWND hwnd) { return hwnd && IsWindow(hwnd) && IsIconic(hwnd); }
bool WindowIsMaximized(HWND hwnd) { return hwnd && IsWindow(hwnd) && IsZoomed(hwnd); }

uint32_t WindowThreadId(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return GetWindowThreadProcessId(hwnd, nullptr);
}

int WindowTextLength(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (int)SendMessageW(hwnd, WM_GETTEXTLENGTH, 0, 0);
}

std::wstring ControlGetText(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return {};
    int len = (int)SendMessageW(hwnd, WM_GETTEXTLENGTH, 0, 0);
    if (len <= 0) return {};
    std::wstring buf(len + 1, L'\0');
    SendMessageW(hwnd, WM_GETTEXT, buf.size(), (LPARAM)buf.data());
    buf.resize(len);
    return buf;
}

bool ControlSetText(HWND hwnd, const std::wstring& text) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    return SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)text.c_str()) != 0;
}

bool WindowEnable(HWND hwnd, bool enable) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    EnableWindow(hwnd, enable ? TRUE : FALSE);
    return true;
}

bool WindowSetFocus(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    DWORD curTid = GetCurrentThreadId();
    DWORD targetTid = GetWindowThreadProcessId(hwnd, nullptr);
    bool attached = false;
    if (curTid != targetTid) attached = AttachThreadInput(curTid, targetTid, TRUE) != 0;
    ::SetFocus(hwnd);
    if (attached) AttachThreadInput(curTid, targetTid, FALSE);
    return true;
}

intptr_t WindowSendMessage(HWND hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (intptr_t)SendMessageW(hwnd, msg, (WPARAM)wParam, (LPARAM)lParam);
}

bool WindowPostMessage(HWND hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    return PostMessageW(hwnd, msg, (WPARAM)wParam, (LPARAM)lParam) != FALSE;
}

bool ButtonClick(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    SendMessageW(hwnd, BM_CLICK, 0, 0);
    return true;
}

int CheckboxGetState(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return -1;
    return (int)SendMessageW(hwnd, BM_GETCHECK, 0, 0);
}

bool CheckboxSetState(HWND hwnd, int state) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    SendMessageW(hwnd, BM_SETCHECK, (WPARAM)state, 0);
    return true;
}

int ComboboxGetCurSel(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return -1;
    return (int)SendMessageW(hwnd, CB_GETCURSEL, 0, 0);
}

bool ComboboxSetCurSel(HWND hwnd, int index) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    return SendMessageW(hwnd, CB_SETCURSEL, (WPARAM)index, 0) != CB_ERR;
}

int ComboboxGetCount(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (int)SendMessageW(hwnd, CB_GETCOUNT, 0, 0);
}

std::wstring ComboboxGetItem(HWND hwnd, int index) {
    if (!hwnd || !IsWindow(hwnd)) return {};
    int len = (int)SendMessageW(hwnd, CB_GETLBTEXTLEN, (WPARAM)index, 0);
    if (len <= 0) return {};
    std::wstring buf(len + 1, L'\0');
    SendMessageW(hwnd, CB_GETLBTEXT, (WPARAM)index, (LPARAM)buf.data());
    buf.resize(len);
    return buf;
}

int ListboxGetCurSel(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return -1;
    return (int)SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
}

bool ListboxSetCurSel(HWND hwnd, int index) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    return SendMessageW(hwnd, LB_SETCURSEL, (WPARAM)index, 0) != LB_ERR;
}

int ListboxGetCount(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (int)SendMessageW(hwnd, LB_GETCOUNT, 0, 0);
}

std::wstring ListboxGetItem(HWND hwnd, int index) {
    if (!hwnd || !IsWindow(hwnd)) return {};
    int len = (int)SendMessageW(hwnd, LB_GETTEXTLEN, (WPARAM)index, 0);
    if (len <= 0) return {};
    std::wstring buf(len + 1, L'\0');
    SendMessageW(hwnd, LB_GETTEXT, (WPARAM)index, (LPARAM)buf.data());
    buf.resize(len);
    return buf;
}

int EditGetLineCount(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (int)SendMessageW(hwnd, EM_GETLINECOUNT, 0, 0);
}

std::wstring EditGetLine(HWND hwnd, int line) {
    if (!hwnd || !IsWindow(hwnd)) return {};
    wchar_t buf[4096]{};
    *(WORD*)buf = sizeof(buf) / sizeof(wchar_t);
    int len = (int)SendMessageW(hwnd, EM_GETLINE, (WPARAM)line, (LPARAM)buf);
    if (len <= 0) return {};
    return std::wstring(buf, len);
}

bool EditSetSel(HWND hwnd, int start, int end) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    SendMessageW(hwnd, EM_SETSEL, (WPARAM)start, (LPARAM)end);
    return true;
}

bool EditReplaceSel(HWND hwnd, const std::wstring& text) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    SendMessageW(hwnd, EM_REPLACESEL, TRUE, (LPARAM)text.c_str());
    return true;
}

int EditGetSel(HWND hwnd, int* startOut, int* endOut) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    DWORD s = 0, e = 0;
    SendMessageW(hwnd, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
    if (startOut) *startOut = (int)s;
    if (endOut) *endOut = (int)e;
    return (int)(e - s);
}

bool ScrollWindow(HWND hwnd, int bar, int pos) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    if (bar == SB_VERT) {
        SendMessageW(hwnd, WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, pos), 0);
    } else {
        SendMessageW(hwnd, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, pos), 0);
    }
    return true;
}

int ScrollGetPos(HWND hwnd, int bar) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return GetScrollPos(hwnd, bar);
}

int ScrollGetRange(HWND hwnd, int bar, int* minOut, int* maxOut) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    int mn = 0, mx = 0;
    GetScrollRange(hwnd, bar, &mn, &mx);
    if (minOut) *minOut = mn;
    if (maxOut) *maxOut = mx;
    return mx - mn;
}

int TabGetCurSel(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return -1;
    return (int)SendMessageW(hwnd, TCM_GETCURSEL, 0, 0);
}

bool TabSetCurSel(HWND hwnd, int index) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    return SendMessageW(hwnd, TCM_SETCURSEL, (WPARAM)index, 0) != -1;
}

int TabGetCount(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (int)SendMessageW(hwnd, TCM_GETITEMCOUNT, 0, 0);
}

int TreeViewGetCount(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (int)SendMessageW(hwnd, TVM_GETCOUNT, 0, 0);
}

intptr_t TreeViewGetSelection(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (intptr_t)SendMessageW(hwnd, TVM_GETNEXTITEM, TVGN_CARET, 0);
}

bool TreeViewSelectItem(HWND hwnd, intptr_t hItem) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    return SendMessageW(hwnd, TVM_SELECTITEM, TVGN_CARET, (LPARAM)hItem) != 0;
}

int ListViewGetItemCount(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (int)SendMessageW(hwnd, LVM_GETITEMCOUNT, 0, 0);
}

int ListViewGetSelectedCount(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    return (int)SendMessageW(hwnd, LVM_GETSELECTEDCOUNT, 0, 0);
}

int ListViewGetNextSelected(HWND hwnd, int start) {
    if (!hwnd || !IsWindow(hwnd)) return -1;
    return (int)SendMessageW(hwnd, LVM_GETNEXTITEM, (WPARAM)start, LVNI_SELECTED);
}

HWND FindChildByClass(HWND parent, const std::wstring& className, int index) {
    if (!parent || !IsWindow(parent)) return nullptr;
    int count = 0;
    HWND child = GetWindow(parent, GW_CHILD);
    while (child) {
        wchar_t cls[256]{};
        GetClassNameW(child, cls, 256);
        if (_wcsicmp(cls, className.c_str()) == 0) {
            if (count == index) return child;
            ++count;
        }
        child = GetWindow(child, GW_HWNDNEXT);
    }
    return nullptr;
}

HWND FindChildByText(HWND parent, const std::wstring& textSubstr) {
    if (!parent || !IsWindow(parent)) return nullptr;
    HWND child = GetWindow(parent, GW_CHILD);
    while (child) {
        std::wstring t = ControlGetText(child);
        if (!textSubstr.empty() && ContainsCaseInsensitive(t, textSubstr)) return child;
        child = GetWindow(child, GW_HWNDNEXT);
    }
    return nullptr;
}

bool ScreenCaptureRect(int x, int y, int w, int h, const std::wstring& bmpPath) {
    if (w <= 0 || h <= 0) return false;
    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return false;
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP bmp = CreateCompatibleBitmap(screenDC, w, h);
    HGDIOBJ old = SelectObject(memDC, bmp);
    BitBlt(memDC, 0, 0, w, h, screenDC, x, y, SRCCOPY);
    SelectObject(memDC, old);

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = -h; // top-down
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    const int rowBytes = ((w * 3 + 3) & ~3);
    const int dataSize = rowBytes * h;
    std::vector<uint8_t> pixels(dataSize);
    GetDIBits(memDC, bmp, 0, h, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    BITMAPFILEHEADER fh{};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(bi);
    fh.bfSize = fh.bfOffBits + dataSize;

    HANDLE file = CreateFileW(bmpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool ok = false;
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, &fh, sizeof(fh), &written, nullptr);
        WriteFile(file, &bi, sizeof(bi), &written, nullptr);
        WriteFile(file, pixels.data(), dataSize, &written, nullptr);
        CloseHandle(file);
        ok = true;
    }

    DeleteObject(bmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
    return ok;
}

int GetMonitorCount() {
    return GetSystemMetrics(SM_CMONITORS);
}

struct MonitorEnumCtx { int target; int current; RECT result; bool found; };
static BOOL CALLBACK MonitorEnumProc(HMONITOR, HDC, LPRECT rc, LPARAM lp) {
    auto* ctx = reinterpret_cast<MonitorEnumCtx*>(lp);
    if (ctx->current == ctx->target) { ctx->result = *rc; ctx->found = true; return FALSE; }
    ctx->current++;
    return TRUE;
}

bool GetMonitorRect(int index, RECT* out) {
    if (!out) return false;
    MonitorEnumCtx ctx{}; ctx.target = index;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&ctx);
    if (ctx.found) { *out = ctx.result; return true; }
    return false;
}

uint32_t GetSystemDpi() {
    HDC dc = GetDC(nullptr);
    uint32_t dpi = dc ? (uint32_t)GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(nullptr, dc);
    return dpi;
}

uint32_t GetWindowDpi(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using Fn = UINT(WINAPI*)(HWND);
        auto* fn = reinterpret_cast<Fn>(GetProcAddress(user32, "GetDpiForWindow"));
        if (fn && hwnd) return fn(hwnd);
    }
    return GetSystemDpi();
}

static bool ParseRegKey(const std::wstring& fullKey, HKEY* rootOut, std::wstring* subKeyOut) {
    size_t sep = fullKey.find(L'\\');
    if (sep == std::wstring::npos) return false;
    std::wstring root = fullKey.substr(0, sep);
    *subKeyOut = fullKey.substr(sep + 1);
    if (root == L"HKLM" || root == L"HKEY_LOCAL_MACHINE") *rootOut = HKEY_LOCAL_MACHINE;
    else if (root == L"HKCU" || root == L"HKEY_CURRENT_USER") *rootOut = HKEY_CURRENT_USER;
    else if (root == L"HKCR" || root == L"HKEY_CLASSES_ROOT") *rootOut = HKEY_CLASSES_ROOT;
    else if (root == L"HKU" || root == L"HKEY_USERS") *rootOut = HKEY_USERS;
    else return false;
    return true;
}

std::wstring RegReadString(const std::wstring& key, const std::wstring& valueName) {
    HKEY root = nullptr; std::wstring subKey;
    if (!ParseRegKey(key, &root, &subKey)) return {};
    HKEY hk = nullptr;
    if (RegOpenKeyExW(root, subKey.c_str(), 0, KEY_READ, &hk) != ERROR_SUCCESS) return {};
    wchar_t buf[4096]{}; DWORD sz = sizeof(buf); DWORD type = 0;
    LSTATUS st = RegQueryValueExW(hk, valueName.c_str(), nullptr, &type, (BYTE*)buf, &sz);
    RegCloseKey(hk);
    if (st != ERROR_SUCCESS || type != REG_SZ) return {};
    return std::wstring(buf);
}

bool RegWriteString(const std::wstring& key, const std::wstring& valueName, const std::wstring& data) {
    HKEY root = nullptr; std::wstring subKey;
    if (!ParseRegKey(key, &root, &subKey)) return false;
    HKEY hk = nullptr;
    if (RegCreateKeyExW(root, subKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hk, nullptr) != ERROR_SUCCESS) return false;
    LSTATUS st = RegSetValueExW(hk, valueName.c_str(), 0, REG_SZ, (const BYTE*)data.c_str(), (DWORD)((data.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hk);
    return st == ERROR_SUCCESS;
}

uint32_t RegReadDword(const std::wstring& key, const std::wstring& valueName, uint32_t defaultVal) {
    HKEY root = nullptr; std::wstring subKey;
    if (!ParseRegKey(key, &root, &subKey)) return defaultVal;
    HKEY hk = nullptr;
    if (RegOpenKeyExW(root, subKey.c_str(), 0, KEY_READ, &hk) != ERROR_SUCCESS) return defaultVal;
    DWORD val = 0, sz = sizeof(val), type = 0;
    LSTATUS st = RegQueryValueExW(hk, valueName.c_str(), nullptr, &type, (BYTE*)&val, &sz);
    RegCloseKey(hk);
    if (st != ERROR_SUCCESS || type != REG_DWORD) return defaultVal;
    return val;
}

bool RegWriteDword(const std::wstring& key, const std::wstring& valueName, uint32_t data) {
    HKEY root = nullptr; std::wstring subKey;
    if (!ParseRegKey(key, &root, &subKey)) return false;
    HKEY hk = nullptr;
    if (RegCreateKeyExW(root, subKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hk, nullptr) != ERROR_SUCCESS) return false;
    LSTATUS st = RegSetValueExW(hk, valueName.c_str(), 0, REG_DWORD, (const BYTE*)&data, sizeof(data));
    RegCloseKey(hk);
    return st == ERROR_SUCCESS;
}

std::wstring EnvGet(const std::wstring& name) {
    wchar_t buf[32768]{};
    DWORD len = GetEnvironmentVariableW(name.c_str(), buf, 32768);
    if (len == 0) return {};
    return std::wstring(buf, len);
}

bool EnvSet(const std::wstring& name, const std::wstring& value) {
    return SetEnvironmentVariableW(name.c_str(), value.c_str()) != FALSE;
}

bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool FileDelete(const std::wstring& path) {
    return DeleteFileW(path.c_str()) != FALSE;
}

bool DirCreate(const std::wstring& path) {
    return CreateDirectoryW(path.c_str(), nullptr) != FALSE || GetLastError() == ERROR_ALREADY_EXISTS;
}

uint64_t FileSize(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return 0;
    return ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
}

int MsgBox(const std::wstring& text, const std::wstring& title, uint32_t flags) {
    return MessageBoxW(nullptr, text.c_str(), title.c_str(), flags);
}

} // namespace winauto
