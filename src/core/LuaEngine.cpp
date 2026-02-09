#include "core/LuaEngine.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
#include <windows.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "core/Humanizer.h"
#include "core/HighPrecisionWait.h"
#include "core/Recorder.h"
#include "core/Replayer.h"

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

static void SendKeyByScanOrVk(uint32_t vk, uint32_t scan, bool extended, bool down) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = (scan != 0) ? 0 : static_cast<WORD>(vk);
    in.ki.wScan = static_cast<WORD>(scan);

    if (scan != 0) {
        in.ki.dwFlags |= KEYEVENTF_SCANCODE;
    } else {
        in.ki.dwFlags |= 0;
        in.ki.wScan = 0;
    }

    if (extended) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    if (!down) in.ki.dwFlags |= KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(in));
}

static void SendTextUtf16(const wchar_t* s, int len) {
    if (!s || len <= 0) return;
    for (int i = 0; i < len; ++i) {
        const wchar_t ch = s[i];
        INPUT in[2]{};
        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wVk = 0;
        in[0].ki.wScan = static_cast<WORD>(ch);
        in[0].ki.dwFlags = KEYEVENTF_UNICODE;

        in[1] = in[0];
        in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
    }
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out;
    out.resize(static_cast<size_t>(len));
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

static std::string ReadFileUtf8(const std::wstring& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

LuaEngine::LuaEngine() = default;

LuaEngine::~LuaEngine() {
    StopAsync();
    Shutdown();
}

bool LuaEngine::Init(Replayer* replayer) {
    if (L_) return true;
    replayer_ = replayer;
    L_ = luaL_newstate();
    if (!L_) return false;
    luaL_openlibs(L_);

    lua_pushlightuserdata(L_, this);
    lua_setglobal(L_, "__acp_self");

    RegisterApi(L_);

    return true;
}

void LuaEngine::Shutdown() {
    StopAsync();
    if (!L_) return;
    lua_close(L_);
    L_ = nullptr;
    replayer_ = nullptr;
}

bool LuaEngine::RunString(const std::string& code, std::string* errorOut) {
    if (!L_) return false;
    if (IsRunning()) return false;
    if (luaL_loadbuffer(L_, code.c_str(), code.size(), "script") != LUA_OK) {
        if (errorOut) *errorOut = lua_tostring(L_, -1);
        lua_pop(L_, 1);
        return false;
    }
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        if (errorOut) *errorOut = lua_tostring(L_, -1);
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

bool LuaEngine::RunFile(const std::wstring& filename, std::string* errorOut) {
    const std::string bytes = ReadFileUtf8(filename);
    if (bytes.empty()) {
        if (errorOut) *errorOut = "failed to read file";
        return false;
    }
    return RunString(bytes, errorOut);
}

bool LuaEngine::StartAsync(const std::string& code) {
    if (!replayer_) return false;
    if (running_.load(std::memory_order_acquire)) return false;
    if (worker_.joinable()) worker_.join();

    cancel_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    currentLine_.store(0, std::memory_order_release);
    {
        std::scoped_lock lock(errorMutex_);
        lastError_.clear();
    }

    targetWindow_ = nullptr;
    std::string patchedCode = code;

    worker_ = std::thread([this, code = std::move(patchedCode)] {
        lua_State* L = luaL_newstate();
        if (!L) {
            std::scoped_lock lock(errorMutex_);
            lastError_ = "failed to create lua state";
            running_.store(false, std::memory_order_release);
            return;
        }

        luaL_openlibs(L);
        lua_pushlightuserdata(L, this);
        lua_setglobal(L, "__acp_self");
        RegisterApi(L);
        lua_sethook(L, &LuaEngine::DebugHook, LUA_MASKLINE, 0);

        if (luaL_loadbuffer(L, code.c_str(), code.size(), "script") != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            {
                std::scoped_lock lock(errorMutex_);
                lastError_ = err ? err : "load error";
            }
            lua_pop(L, 1);
            lua_close(L);
            running_.store(false, std::memory_order_release);
            return;
        }

        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            {
                std::scoped_lock lock(errorMutex_);
                lastError_ = err ? err : "runtime error";
            }
            lua_pop(L, 1);
        }

        lua_close(L);
        running_.store(false, std::memory_order_release);
    });
    return true;
}

void LuaEngine::StopAsync() {
    cancel_.store(true, std::memory_order_release);
    if (worker_.joinable()) worker_.join();
    running_.store(false, std::memory_order_release);
}

bool LuaEngine::IsRunning() const {
    return running_.load(std::memory_order_acquire);
}

int LuaEngine::CurrentLine() const {
    return currentLine_.load(std::memory_order_acquire);
}

std::string LuaEngine::LastError() const {
    std::scoped_lock lock(errorMutex_);
    return lastError_;
}

LuaEngine* LuaEngine::Self(lua_State* L) {
    lua_getglobal(L, "__acp_self");
    auto* self = static_cast<LuaEngine*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return self;
}

void LuaEngine::RegisterApi(lua_State* L) {
    lua_register(L, "playback", &LuaEngine::L_Playback);
    lua_register(L, "human_move", &LuaEngine::L_HumanMove);
    lua_register(L, "human_click", &LuaEngine::L_HumanClick);
    lua_register(L, "human_scroll", &LuaEngine::L_HumanScroll);
    lua_register(L, "set_speed", &LuaEngine::L_SetSpeed);
    lua_register(L, "wait_ms", &LuaEngine::L_WaitMs);
    lua_register(L, "wait_us", &LuaEngine::L_WaitUs);
    lua_register(L, "mouse_move", &LuaEngine::L_MouseMove);
    lua_register(L, "mouse_down", &LuaEngine::L_MouseDown);
    lua_register(L, "mouse_up", &LuaEngine::L_MouseUp);
    lua_register(L, "mouse_wheel", &LuaEngine::L_MouseWheel);
    lua_register(L, "key_down", &LuaEngine::L_KeyDown);
    lua_register(L, "key_up", &LuaEngine::L_KeyUp);
    lua_register(L, "vk_down", &LuaEngine::L_VkDown);
    lua_register(L, "vk_up", &LuaEngine::L_VkUp);
    lua_register(L, "vk_press", &LuaEngine::L_VkPress);
    lua_register(L, "text", &LuaEngine::L_Text);
    lua_register(L, "set_target_window", &LuaEngine::L_SetTargetWindow);
    lua_register(L, "clear_target_window", &LuaEngine::L_ClearTargetWindow);
}

void LuaEngine::DebugHook(lua_State* L, lua_Debug* ar) {
    if (!ar) return;
    if (ar->event != LUA_HOOKLINE) return;

    auto* self = Self(L);
    if (!self) return;
    self->currentLine_.store(ar->currentline, std::memory_order_release);
    if (self->cancel_.load(std::memory_order_acquire)) {
        luaL_error(L, "cancelled");
    }
}

void LuaEngine::WaitMicrosCancelable(int64_t us) {
    if (us <= 0) return;
    const int64_t start = timing::MicrosNow();
    while (true) {
        if (cancel_.load(std::memory_order_acquire)) return;
        const int64_t elapsed = timing::MicrosNow() - start;
        const int64_t remaining = us - elapsed;
        if (remaining <= 0) break;
        if (remaining > 2000) {
            Sleep(1);
        } else {
            std::this_thread::yield();
        }
    }
}

int LuaEngine::L_Playback(lua_State* L) {
    auto* self = Self(L);
    if (!self || !self->replayer_) return 0;
    const char* s = luaL_checkstring(L, 1);
    std::wstring filename = Utf8ToWide(s ? s : "");

    Recorder tmp;
    if (!tmp.LoadFromFile(filename)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const auto& eventsRef = tmp.Events();
    std::vector<trc::RawEvent> events(eventsRef.begin(), eventsRef.end());
    const bool started = self->replayer_->Start(std::move(events), false, self->replayer_->Speed());
    lua_pushboolean(L, started ? 1 : 0);
    return 1;
}

int LuaEngine::L_HumanMove(lua_State* L) {
    const int x = static_cast<int>(luaL_checkinteger(L, 1));
    const int y = static_cast<int>(luaL_checkinteger(L, 2));
    const double speed = lua_gettop(L) >= 3 ? luaL_checknumber(L, 3) : 1.0;
    human::MoveTo(x, y, speed);
    return 0;
}

static int ParseButton(lua_State* L, int idx) {
    if (lua_type(L, idx) == LUA_TNUMBER) return static_cast<int>(lua_tointeger(L, idx));
    const char* s = luaL_checkstring(L, idx);
    if (!s) return 1;
    if (_stricmp(s, "left") == 0) return 1;
    if (_stricmp(s, "right") == 0) return 2;
    if (_stricmp(s, "middle") == 0) return 3;
    return 1;
}

int LuaEngine::L_HumanClick(lua_State* L) {
    const int btn = ParseButton(L, 1);
    human::Click(btn);
    return 0;
}

int LuaEngine::L_HumanScroll(lua_State* L) {
    const int delta = static_cast<int>(luaL_checkinteger(L, 1));
    human::Scroll(delta);
    return 0;
}

int LuaEngine::L_SetSpeed(lua_State* L) {
    auto* self = Self(L);
    if (!self || !self->replayer_) return 0;
    const double factor = luaL_checknumber(L, 1);
    self->replayer_->SetSpeed(factor);
    return 0;
}

int LuaEngine::L_WaitMs(lua_State* L) {
    auto* self = Self(L);
    if (!self) return 0;
    const int64_t ms = static_cast<int64_t>(luaL_checkinteger(L, 1));
    self->WaitMicrosCancelable(std::max<int64_t>(0, ms) * 1000);
    if (self->cancel_.load(std::memory_order_acquire)) luaL_error(L, "cancelled");
    return 0;
}

int LuaEngine::L_WaitUs(lua_State* L) {
    auto* self = Self(L);
    if (!self) return 0;
    const int64_t us = static_cast<int64_t>(luaL_checkinteger(L, 1));
    self->WaitMicrosCancelable(std::max<int64_t>(0, us));
    if (self->cancel_.load(std::memory_order_acquire)) luaL_error(L, "cancelled");
    return 0;
}

int LuaEngine::L_MouseMove(lua_State* L) {
    const int x = static_cast<int>(luaL_checkinteger(L, 1));
    const int y = static_cast<int>(luaL_checkinteger(L, 2));
    MoveCursorBestEffort(x, y);
    if (auto* self = Self(L)) {
        self->hasLastMouse_ = true;
        self->lastMouseX_ = x;
        self->lastMouseY_ = y;
    }
    return 0;
}

static bool LuaTryGetXY(lua_State* L, int idxX, int idxY, int* xOut, int* yOut) {
    if (lua_gettop(L) < idxY) return false;
    if (lua_type(L, idxX) != LUA_TNUMBER || lua_type(L, idxY) != LUA_TNUMBER) return false;
    if (xOut) *xOut = static_cast<int>(lua_tointeger(L, idxX));
    if (yOut) *yOut = static_cast<int>(lua_tointeger(L, idxY));
    return true;
}

int LuaEngine::L_MouseDown(lua_State* L) {
    const int btn = ParseButton(L, 1);
    int x = 0, y = 0;
    if (LuaTryGetXY(L, 2, 3, &x, &y)) {
        MoveCursorBestEffort(x, y);
        if (auto* self = Self(L)) {
            self->hasLastMouse_ = true;
            self->lastMouseX_ = x;
            self->lastMouseY_ = y;
        }
    } else {
        POINT pt{};
        if (GetCursorPos(&pt)) {
            if (auto* self = Self(L)) {
                self->hasLastMouse_ = true;
                self->lastMouseX_ = pt.x;
                self->lastMouseY_ = pt.y;
            }
        }
    }

    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MouseDownFlag(btn);
    if (in.mi.dwFlags == 0) return 0;
    if (btn == 4 || btn == 5) in.mi.mouseData = MouseXButtonData(btn);
    SendInput(1, &in, sizeof(in));
    return 0;
}

int LuaEngine::L_MouseUp(lua_State* L) {
    const int btn = ParseButton(L, 1);
    int x = 0, y = 0;
    if (LuaTryGetXY(L, 2, 3, &x, &y)) {
        MoveCursorBestEffort(x, y);
        if (auto* self = Self(L)) {
            self->hasLastMouse_ = true;
            self->lastMouseX_ = x;
            self->lastMouseY_ = y;
        }
    } else {
        POINT pt{};
        if (GetCursorPos(&pt)) {
            if (auto* self = Self(L)) {
                self->hasLastMouse_ = true;
                self->lastMouseX_ = pt.x;
                self->lastMouseY_ = pt.y;
            }
        }
    }

    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MouseUpFlag(btn);
    if (in.mi.dwFlags == 0) return 0;
    if (btn == 4 || btn == 5) in.mi.mouseData = MouseXButtonData(btn);
    SendInput(1, &in, sizeof(in));
    return 0;
}

int LuaEngine::L_MouseWheel(lua_State* L) {
    const int delta = static_cast<int>(luaL_checkinteger(L, 1));
    int x = 0, y = 0;
    if (LuaTryGetXY(L, 2, 3, &x, &y)) {
        MoveCursorBestEffort(x, y);
        if (auto* self = Self(L)) {
            self->hasLastMouse_ = true;
            self->lastMouseX_ = x;
            self->lastMouseY_ = y;
        }
    } else {
        POINT pt{};
        if (GetCursorPos(&pt)) {
            if (auto* self = Self(L)) {
                self->hasLastMouse_ = true;
                self->lastMouseX_ = pt.x;
                self->lastMouseY_ = pt.y;
            }
        }
    }

    const bool horizontal = lua_gettop(L) >= 4 ? (lua_toboolean(L, 4) != 0) : false;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = horizontal ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL;
    const int scaled = (std::abs(delta) < WHEEL_DELTA) ? (delta * WHEEL_DELTA) : delta;
    in.mi.mouseData = static_cast<DWORD>(scaled);
    SendInput(1, &in, sizeof(in));
    return 0;
}

static void SendKeyScancode(int scanCode, bool extended, bool down) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = 0;
    in.ki.wScan = static_cast<WORD>(scanCode);
    in.ki.dwFlags = KEYEVENTF_SCANCODE;
    if (extended) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    if (!down) in.ki.dwFlags |= KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(in));
}

int LuaEngine::L_KeyDown(lua_State* L) {
    const int scan = static_cast<int>(luaL_checkinteger(L, 1));
    const bool ext = lua_gettop(L) >= 2 ? (lua_toboolean(L, 2) != 0) : false;
    SendKeyScancode(scan, ext, true);
    return 0;
}

int LuaEngine::L_KeyUp(lua_State* L) {
    const int scan = static_cast<int>(luaL_checkinteger(L, 1));
    const bool ext = lua_gettop(L) >= 2 ? (lua_toboolean(L, 2) != 0) : false;
    SendKeyScancode(scan, ext, false);
    return 0;
}

int LuaEngine::L_VkDown(lua_State* L) {
    const uint32_t vk = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const bool ext = lua_gettop(L) >= 2 ? (lua_toboolean(L, 2) != 0) : false;
    SendKeyByScanOrVk(vk, 0, ext, true);
    return 0;
}

int LuaEngine::L_VkUp(lua_State* L) {
    const uint32_t vk = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const bool ext = lua_gettop(L) >= 2 ? (lua_toboolean(L, 2) != 0) : false;
    SendKeyByScanOrVk(vk, 0, ext, false);
    return 0;
}

int LuaEngine::L_VkPress(lua_State* L) {
    const uint32_t vk = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const int64_t holdMs = (lua_gettop(L) >= 2) ? static_cast<int64_t>(luaL_checkinteger(L, 2)) : 60;
    const bool ext = lua_gettop(L) >= 3 ? (lua_toboolean(L, 3) != 0) : false;
    SendKeyByScanOrVk(vk, 0, ext, true);
    auto* self = Self(L);
    if (self) {
        self->WaitMicrosCancelable(std::max<int64_t>(0, holdMs) * 1000);
        if (self->cancel_.load(std::memory_order_acquire)) luaL_error(L, "cancelled");
    } else {
        timing::HighPrecisionWaitMicros(std::max<int64_t>(0, holdMs) * 1000);
    }
    SendKeyByScanOrVk(vk, 0, ext, false);
    return 0;
}

int LuaEngine::L_Text(lua_State* L) {
    auto* self = Self(L);
    const char* s = luaL_checkstring(L, 1);
    if (!s) return 0;
    const int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (len <= 1) return 0;
    std::wstring w;
    w.resize(static_cast<size_t>(len));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), len);
    SendTextUtf16(w.c_str(), static_cast<int>(w.size() - 1));
    if (self && self->cancel_.load(std::memory_order_acquire)) luaL_error(L, "cancelled");
    return 0;
}

int LuaEngine::L_SetTargetWindow(lua_State* L) {
    auto* self = Self(L);
    if (!self) return 0;
    const int x = static_cast<int>(luaL_checkinteger(L, 1));
    const int y = static_cast<int>(luaL_checkinteger(L, 2));
    self->hasLastMouse_ = true;
    self->lastMouseX_ = x;
    self->lastMouseY_ = y;
    self->targetWindow_ = nullptr;
    return 0;
}

int LuaEngine::L_ClearTargetWindow(lua_State* L) {
    auto* self = Self(L);
    if (!self) return 0;
    self->targetWindow_ = nullptr;
    return 0;
}
