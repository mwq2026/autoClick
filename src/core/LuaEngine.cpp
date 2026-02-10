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
#include "core/WinAutomation.h"

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

static void SendMouseWheelBestEffort(int delta, bool horizontal) {
    const int scaled = (std::abs(delta) < WHEEL_DELTA) ? (delta * WHEEL_DELTA) : delta;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = horizontal ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL;
    in.mi.mouseData = static_cast<DWORD>(scaled);
    SendInput(1, &in, sizeof(in));
}

static bool FocusWindowAt(int x, int y) {
    POINT pt{ x, y };
    HWND hwnd = RootWindowAtSkipSelf(pt);
    if (!hwnd) return false;

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

static void LuaPushHwnd(lua_State* L, HWND hwnd) {
    lua_pushinteger(L, static_cast<lua_Integer>(reinterpret_cast<uintptr_t>(hwnd)));
}

static HWND LuaToHwnd(lua_State* L, int idx) {
    if (!L) return nullptr;
    if (lua_type(L, idx) != LUA_TNUMBER) return nullptr;
    const auto v = static_cast<uintptr_t>(lua_tointeger(L, idx));
    return reinterpret_cast<HWND>(v);
}

static std::string ReadFileUtf8(const std::wstring& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

const std::vector<LuaEngine::LuaApiDoc>& LuaEngine::ApiDocs() {
    static const std::vector<LuaApiDoc> docs = {
        { "playback", "playback(path_trc)", "回放", "回放一个 .trc 文件" },
        { "human_move", "human_move(x, y[, duration_ms])", "拟人", "拟人方式移动鼠标" },
        { "human_click", "human_click(btn[, x, y])", "拟人", "拟人方式点击鼠标" },
        { "human_scroll", "human_scroll(delta[, x, y])", "拟人", "拟人方式滚动" },

        { "set_speed", "set_speed(factor)", "基础", "设置脚本执行速度倍率" },
        { "wait_ms", "wait_ms(ms)", "基础", "等待指定毫秒" },
        { "wait_us", "wait_us(us)", "基础", "等待指定微秒" },

        { "activate_window", "activate_window([x, y]) -> boolean", "窗口", "按坐标激活顶层窗口（兼容旧脚本）" },
        { "window_is_valid", "window_is_valid(hwnd) -> boolean", "窗口", "判断窗口句柄是否有效" },
        { "window_from_point", "window_from_point(x, y) -> hwnd|nil", "窗口", "获取坐标处顶层窗口（默认跳过本程序）" },
        { "window_foreground", "window_foreground() -> hwnd|nil", "窗口", "获取当前前台窗口（默认跳过本程序）" },
        { "window_find", "window_find(title_substr[, class_substr[, visible_only[, skip_self]]]) -> hwnd|nil", "窗口", "按标题/类名模糊查找顶层窗口" },
        { "window_find_all", "window_find_all(title_substr[, class_substr[, visible_only[, skip_self]]]) -> {hwnd,...}", "窗口", "按标题/类名模糊查找所有匹配窗口" },
        { "window_wait", "window_wait(title_substr, timeout_ms[, interval_ms[, class_substr[, visible_only[, skip_self]]]]) -> hwnd|nil", "窗口", "等待窗口出现" },
        { "window_title", "window_title(hwnd) -> string|nil", "窗口", "读取窗口标题" },
        { "window_class", "window_class(hwnd) -> string|nil", "窗口", "读取窗口类名" },
        { "window_pid", "window_pid(hwnd) -> pid|nil", "窗口", "获取窗口进程 PID" },
        { "window_rect", "window_rect(hwnd) -> x, y, w, h|nil", "窗口", "获取窗口矩形（屏幕坐标）" },
        { "window_client_rect", "window_client_rect(hwnd) -> w, h|nil", "窗口", "获取客户区大小" },
        { "window_activate", "window_activate(hwnd) -> boolean", "窗口", "激活并置前窗口" },
        { "window_activate_at", "window_activate_at(x, y) -> boolean", "窗口", "按坐标定位并激活顶层窗口" },
        { "window_set_topmost", "window_set_topmost(hwnd, on) -> boolean", "窗口", "设置/取消窗口置顶" },
        { "window_bring_to_top", "window_bring_to_top(hwnd) -> boolean", "窗口", "把窗口放到最前（不置顶）" },
        { "window_send_to_back", "window_send_to_back(hwnd) -> boolean", "窗口", "把窗口放到最后" },
        { "window_show", "window_show(hwnd) -> boolean", "窗口", "显示窗口" },
        { "window_hide", "window_hide(hwnd) -> boolean", "窗口", "隐藏窗口" },
        { "window_minimize", "window_minimize(hwnd) -> boolean", "窗口", "最小化窗口" },
        { "window_maximize", "window_maximize(hwnd) -> boolean", "窗口", "最大化窗口" },
        { "window_restore", "window_restore(hwnd) -> boolean", "窗口", "还原窗口" },
        { "window_move", "window_move(hwnd, x, y) -> boolean", "窗口", "移动窗口到屏幕坐标" },
        { "window_resize", "window_resize(hwnd, w, h) -> boolean", "窗口", "调整窗口尺寸" },
        { "window_set_rect", "window_set_rect(hwnd, x, y, w, h) -> boolean", "窗口", "移动并调整窗口尺寸" },
        { "window_close", "window_close(hwnd) -> boolean", "窗口", "请求关闭窗口（温和）" },
        { "window_close_force", "window_close_force(hwnd[, wait_ms]) -> boolean", "窗口", "高风险：超时后强制结束进程" },

        { "process_start", "process_start(path[, args[, cwd]]) -> pid|nil", "进程", "启动进程（CreateProcess）" },
        { "process_is_running", "process_is_running(pid) -> boolean", "进程", "判断进程是否仍在运行" },
        { "process_wait", "process_wait(pid, timeout_ms) -> boolean", "进程", "等待进程退出" },
        { "process_kill", "process_kill(pid[, exit_code]) -> boolean", "进程", "高风险：强制结束进程" },

        { "clipboard_set", "clipboard_set(text_utf8) -> boolean", "系统", "写入剪贴板文本" },
        { "clipboard_get", "clipboard_get() -> string|nil", "系统", "读取剪贴板文本" },
        { "screen_size", "screen_size() -> w, h|nil", "系统", "获取虚拟屏幕大小" },
        { "cursor_pos", "cursor_pos() -> x, y|nil", "系统", "获取鼠标当前位置" },
        { "cursor_set", "cursor_set(x, y) -> boolean", "系统", "设置鼠标当前位置" },

        { "pixel_get", "pixel_get(x, y) -> r, g, b|nil", "视觉", "获取屏幕像素颜色" },
        { "color_wait", "color_wait(x, y, r, g, b[, tol[, timeout_ms[, interval_ms]]]) -> boolean", "视觉", "等待屏幕像素达到目标颜色" },

        { "mouse_move", "mouse_move(x, y)", "输入", "移动鼠标到坐标" },
        { "mouse_down", "mouse_down(btn[, x, y])", "输入", "按下鼠标按键" },
        { "mouse_up", "mouse_up(btn[, x, y])", "输入", "抬起鼠标按键" },
        { "mouse_wheel", "mouse_wheel(delta[, x, y[, horizontal]])", "输入", "滚轮（horizontal 建议传 0/1）" },

        { "key_down", "key_down(scan[, ext])", "输入", "按下扫描码键（ext 建议传 0/1）" },
        { "key_up", "key_up(scan[, ext])", "输入", "抬起扫描码键（ext 建议传 0/1）" },
        { "vk_down", "vk_down(vk_or_name[, ext])", "输入", "按下 VK 键" },
        { "vk_up", "vk_up(vk_or_name[, ext])", "输入", "抬起 VK 键" },
        { "vk_press", "vk_press(vk_or_char[, hold_ms[, ext]])", "输入", "按下并抬起 VK/字符" },
        { "text", "text(str_utf8)", "输入", "输入 UTF-8 文本" },

        { "set_target_window", "set_target_window(hwnd)", "高级", "设置目标窗口（供部分模式使用）" },
        { "clear_target_window", "clear_target_window()", "高级", "清除目标窗口" },
    };
    return docs;
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
    lua_register(L, "activate_window", &LuaEngine::L_ActivateWindow);
    lua_register(L, "window_is_valid", &LuaEngine::L_WindowIsValid);
    lua_register(L, "window_from_point", &LuaEngine::L_WindowFromPoint);
    lua_register(L, "window_foreground", &LuaEngine::L_WindowForeground);
    lua_register(L, "window_find", &LuaEngine::L_WindowFind);
    lua_register(L, "window_find_all", &LuaEngine::L_WindowFindAll);
    lua_register(L, "window_wait", &LuaEngine::L_WindowWait);
    lua_register(L, "window_title", &LuaEngine::L_WindowTitle);
    lua_register(L, "window_class", &LuaEngine::L_WindowClass);
    lua_register(L, "window_pid", &LuaEngine::L_WindowPid);
    lua_register(L, "window_rect", &LuaEngine::L_WindowRect);
    lua_register(L, "window_client_rect", &LuaEngine::L_WindowClientRect);
    lua_register(L, "window_activate", &LuaEngine::L_WindowActivate);
    lua_register(L, "window_activate_at", &LuaEngine::L_WindowActivateAt);
    lua_register(L, "window_set_topmost", &LuaEngine::L_WindowSetTopmost);
    lua_register(L, "window_bring_to_top", &LuaEngine::L_WindowBringToTop);
    lua_register(L, "window_send_to_back", &LuaEngine::L_WindowSendToBack);
    lua_register(L, "window_show", &LuaEngine::L_WindowShow);
    lua_register(L, "window_hide", &LuaEngine::L_WindowHide);
    lua_register(L, "window_minimize", &LuaEngine::L_WindowMinimize);
    lua_register(L, "window_maximize", &LuaEngine::L_WindowMaximize);
    lua_register(L, "window_restore", &LuaEngine::L_WindowRestore);
    lua_register(L, "window_move", &LuaEngine::L_WindowMove);
    lua_register(L, "window_resize", &LuaEngine::L_WindowResize);
    lua_register(L, "window_set_rect", &LuaEngine::L_WindowSetRect);
    lua_register(L, "window_close", &LuaEngine::L_WindowClose);
    lua_register(L, "window_close_force", &LuaEngine::L_WindowCloseForce);
    lua_register(L, "process_start", &LuaEngine::L_ProcessStart);
    lua_register(L, "process_is_running", &LuaEngine::L_ProcessIsRunning);
    lua_register(L, "process_wait", &LuaEngine::L_ProcessWait);
    lua_register(L, "process_kill", &LuaEngine::L_ProcessKill);
    lua_register(L, "clipboard_set", &LuaEngine::L_ClipboardSet);
    lua_register(L, "clipboard_get", &LuaEngine::L_ClipboardGet);
    lua_register(L, "screen_size", &LuaEngine::L_ScreenSize);
    lua_register(L, "cursor_pos", &LuaEngine::L_CursorPos);
    lua_register(L, "cursor_set", &LuaEngine::L_CursorSet);
    lua_register(L, "pixel_get", &LuaEngine::L_PixelGet);
    lua_register(L, "color_wait", &LuaEngine::L_ColorWait);
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

static bool LuaBool01(lua_State* L, int idx, bool defaultValue);

int LuaEngine::L_ActivateWindow(lua_State* L) {
    auto* self = Self(L);
    int x = 0, y = 0;
    if (LuaTryGetXY(L, 1, 2, &x, &y)) {
        if (self) {
            self->hasLastMouse_ = true;
            self->lastMouseX_ = x;
            self->lastMouseY_ = y;
        }
    } else if (self && self->hasLastMouse_) {
        x = self->lastMouseX_;
        y = self->lastMouseY_;
    } else {
        POINT pt{};
        if (!GetCursorPos(&pt)) {
            lua_pushboolean(L, 0);
            return 1;
        }
        x = pt.x;
        y = pt.y;
        if (self) {
            self->hasLastMouse_ = true;
            self->lastMouseX_ = x;
            self->lastMouseY_ = y;
        }
    }

    const bool ok = FocusWindowAt(x, y);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowIsValid(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    lua_pushboolean(L, (hwnd && IsWindow(hwnd) != FALSE) ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowFromPoint(lua_State* L) {
    const int x = static_cast<int>(luaL_checkinteger(L, 1));
    const int y = static_cast<int>(luaL_checkinteger(L, 2));
    POINT pt{ x, y };
    HWND hwnd = winauto::RootWindowAtSkipSelf(pt);
    if (!hwnd) {
        lua_pushnil(L);
        return 1;
    }
    LuaPushHwnd(L, hwnd);
    return 1;
}

int LuaEngine::L_WindowForeground(lua_State* L) {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || IsWindow(hwnd) == FALSE) {
        lua_pushnil(L);
        return 1;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != 0 && pid == GetCurrentProcessId()) {
        lua_pushnil(L);
        return 1;
    }
    LuaPushHwnd(L, hwnd);
    return 1;
}

static void LuaPushHwndTable(lua_State* L, const std::vector<HWND>& hwnds) {
    lua_createtable(L, static_cast<int>(hwnds.size()), 0);
    int idx = 1;
    for (HWND h : hwnds) {
        LuaPushHwnd(L, h);
        lua_rawseti(L, -2, idx++);
    }
}

int LuaEngine::L_WindowFind(lua_State* L) {
    const char* titleUtf8 = luaL_checkstring(L, 1);
    const std::wstring title = Utf8ToWide(titleUtf8 ? titleUtf8 : "");
    const std::wstring cls = (lua_gettop(L) >= 2 && lua_type(L, 2) == LUA_TSTRING) ? Utf8ToWide(lua_tostring(L, 2)) : std::wstring{};
    const bool visibleOnly = (lua_gettop(L) >= 3) ? LuaBool01(L, 3, true) : true;
    const bool skipSelf = (lua_gettop(L) >= 4) ? LuaBool01(L, 4, true) : true;

    const auto hwnds = winauto::FindWindowsByTitleContains(title, cls, visibleOnly, skipSelf);
    if (hwnds.empty()) {
        lua_pushnil(L);
        return 1;
    }
    LuaPushHwnd(L, hwnds.front());
    return 1;
}

int LuaEngine::L_WindowFindAll(lua_State* L) {
    const char* titleUtf8 = luaL_checkstring(L, 1);
    const std::wstring title = Utf8ToWide(titleUtf8 ? titleUtf8 : "");
    const std::wstring cls = (lua_gettop(L) >= 2 && lua_type(L, 2) == LUA_TSTRING) ? Utf8ToWide(lua_tostring(L, 2)) : std::wstring{};
    const bool visibleOnly = (lua_gettop(L) >= 3) ? LuaBool01(L, 3, true) : true;
    const bool skipSelf = (lua_gettop(L) >= 4) ? LuaBool01(L, 4, true) : true;

    const auto hwnds = winauto::FindWindowsByTitleContains(title, cls, visibleOnly, skipSelf);
    LuaPushHwndTable(L, hwnds);
    return 1;
}

int LuaEngine::L_WindowWait(lua_State* L) {
    auto* self = Self(L);
    const char* titleUtf8 = luaL_checkstring(L, 1);
    const int64_t timeoutMs = static_cast<int64_t>(luaL_checkinteger(L, 2));
    const int64_t intervalMs = (lua_gettop(L) >= 3) ? static_cast<int64_t>(luaL_checkinteger(L, 3)) : 50;
    const std::wstring cls = (lua_gettop(L) >= 4 && lua_type(L, 4) == LUA_TSTRING) ? Utf8ToWide(lua_tostring(L, 4)) : std::wstring{};
    const bool visibleOnly = (lua_gettop(L) >= 5) ? LuaBool01(L, 5, true) : true;
    const bool skipSelf = (lua_gettop(L) >= 6) ? LuaBool01(L, 6, true) : true;

    const std::wstring title = Utf8ToWide(titleUtf8 ? titleUtf8 : "");
    const int64_t deadline = timing::MicrosNow() + std::max<int64_t>(0, timeoutMs) * 1000;
    while (timing::MicrosNow() <= deadline) {
        const auto hwnds = winauto::FindWindowsByTitleContains(title, cls, visibleOnly, skipSelf);
        if (!hwnds.empty()) {
            LuaPushHwnd(L, hwnds.front());
            return 1;
        }
        if (self) {
            self->WaitMicrosCancelable(std::max<int64_t>(0, intervalMs) * 1000);
            if (self->cancel_.load(std::memory_order_acquire)) luaL_error(L, "cancelled");
        } else {
            Sleep(static_cast<DWORD>(std::clamp<int64_t>(intervalMs, 0, 1000)));
        }
    }
    lua_pushnil(L);
    return 1;
}

int LuaEngine::L_WindowTitle(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const std::wstring title = winauto::WindowTitle(hwnd);
    if (title.empty()) {
        lua_pushnil(L);
        return 1;
    }
    const int len = WideCharToMultiByte(CP_UTF8, 0, title.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) {
        lua_pushnil(L);
        return 1;
    }
    std::string out;
    out.resize(static_cast<size_t>(len));
    WideCharToMultiByte(CP_UTF8, 0, title.c_str(), -1, out.data(), len, nullptr, nullptr);
    lua_pushlstring(L, out.c_str(), out.size() - 1);
    return 1;
}

int LuaEngine::L_WindowClass(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const std::wstring cls = winauto::WindowClass(hwnd);
    if (cls.empty()) {
        lua_pushnil(L);
        return 1;
    }
    const int len = WideCharToMultiByte(CP_UTF8, 0, cls.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) {
        lua_pushnil(L);
        return 1;
    }
    std::string out;
    out.resize(static_cast<size_t>(len));
    WideCharToMultiByte(CP_UTF8, 0, cls.c_str(), -1, out.data(), len, nullptr, nullptr);
    lua_pushlstring(L, out.c_str(), out.size() - 1);
    return 1;
}

int LuaEngine::L_WindowPid(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const uint32_t pid = winauto::WindowPid(hwnd);
    if (pid == 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(pid));
    return 1;
}

int LuaEngine::L_WindowRect(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    RECT rc{};
    if (!winauto::WindowRect(hwnd, &rc)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, rc.left);
    lua_pushinteger(L, rc.top);
    lua_pushinteger(L, rc.right - rc.left);
    lua_pushinteger(L, rc.bottom - rc.top);
    return 4;
}

int LuaEngine::L_WindowClientRect(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    int w = 0, h = 0;
    if (!winauto::WindowClientSize(hwnd, &w, &h)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
}

int LuaEngine::L_WindowActivate(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const bool ok = winauto::ActivateWindow(hwnd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowActivateAt(lua_State* L) {
    const int x = static_cast<int>(luaL_checkinteger(L, 1));
    const int y = static_cast<int>(luaL_checkinteger(L, 2));
    POINT pt{ x, y };
    HWND hwnd = winauto::RootWindowAtSkipSelf(pt);
    const bool ok = winauto::ActivateWindow(hwnd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowSetTopmost(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const bool on = LuaBool01(L, 2, true);
    const bool ok = winauto::WindowSetTopmost(hwnd, on);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowBringToTop(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const bool ok = winauto::WindowBringToTop(hwnd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowSendToBack(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const bool ok = winauto::WindowSendToBack(hwnd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowShow(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const bool ok = winauto::WindowShow(hwnd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowHide(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const bool ok = winauto::WindowHide(hwnd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowMinimize(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const bool ok = winauto::WindowMinimize(hwnd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowMaximize(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const bool ok = winauto::WindowMaximize(hwnd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowRestore(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const bool ok = winauto::WindowRestore(hwnd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowMove(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const int x = static_cast<int>(luaL_checkinteger(L, 2));
    const int y = static_cast<int>(luaL_checkinteger(L, 3));
    const bool ok = winauto::WindowMove(hwnd, x, y);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowResize(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const int w = static_cast<int>(luaL_checkinteger(L, 2));
    const int h = static_cast<int>(luaL_checkinteger(L, 3));
    const bool ok = winauto::WindowResize(hwnd, w, h);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowSetRect(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const int x = static_cast<int>(luaL_checkinteger(L, 2));
    const int y = static_cast<int>(luaL_checkinteger(L, 3));
    const int w = static_cast<int>(luaL_checkinteger(L, 4));
    const int h = static_cast<int>(luaL_checkinteger(L, 5));
    const bool ok = winauto::WindowSetRect(hwnd, x, y, w, h);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowClose(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const bool ok = winauto::WindowClose(hwnd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_WindowCloseForce(lua_State* L) {
    const HWND hwnd = LuaToHwnd(L, 1);
    const uint32_t waitMs = (lua_gettop(L) >= 2) ? static_cast<uint32_t>(luaL_checkinteger(L, 2)) : 500;
    const bool ok = winauto::WindowCloseForce(hwnd, waitMs);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_ProcessStart(lua_State* L) {
    const char* pathUtf8 = luaL_checkstring(L, 1);
    const std::wstring path = Utf8ToWide(pathUtf8 ? pathUtf8 : "");
    const std::wstring args = (lua_gettop(L) >= 2 && lua_type(L, 2) == LUA_TSTRING) ? Utf8ToWide(lua_tostring(L, 2)) : std::wstring{};
    const std::wstring cwd = (lua_gettop(L) >= 3 && lua_type(L, 3) == LUA_TSTRING) ? Utf8ToWide(lua_tostring(L, 3)) : std::wstring{};

    const uint32_t pid = winauto::ProcessStart(path, args, cwd);
    if (pid == 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(pid));
    return 1;
}

int LuaEngine::L_ProcessIsRunning(lua_State* L) {
    const uint32_t pid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const bool ok = winauto::ProcessIsRunning(pid);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_ProcessWait(lua_State* L) {
    const uint32_t pid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const uint32_t timeoutMs = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    const bool ok = winauto::ProcessWait(pid, timeoutMs);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_ProcessKill(lua_State* L) {
    const uint32_t pid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const uint32_t exitCode = (lua_gettop(L) >= 2) ? static_cast<uint32_t>(luaL_checkinteger(L, 2)) : 1;
    const bool ok = winauto::ProcessKill(pid, exitCode);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static bool WideToUtf8(const std::wstring& w, std::string* out) {
    if (!out) return false;
    out->clear();
    if (w.empty()) return true;
    const int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return false;
    out->resize(static_cast<size_t>(len));
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out->data(), len, nullptr, nullptr);
    out->resize(out->size() - 1);
    return true;
}

int LuaEngine::L_ClipboardSet(lua_State* L) {
    const char* s = luaL_checkstring(L, 1);
    const std::wstring w = Utf8ToWide(s ? s : "");
    const bool ok = winauto::ClipboardSetText(w);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_ClipboardGet(lua_State* L) {
    const std::wstring w = winauto::ClipboardGetText();
    std::string out;
    if (!WideToUtf8(w, &out) || out.empty()) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, out.c_str(), out.size());
    return 1;
}

int LuaEngine::L_ScreenSize(lua_State* L) {
    int w = 0, h = 0;
    if (!winauto::ScreenSize(&w, &h)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
}

int LuaEngine::L_CursorPos(lua_State* L) {
    POINT pt{};
    if (!winauto::CursorPos(&pt)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, pt.x);
    lua_pushinteger(L, pt.y);
    return 2;
}

int LuaEngine::L_CursorSet(lua_State* L) {
    const int x = static_cast<int>(luaL_checkinteger(L, 1));
    const int y = static_cast<int>(luaL_checkinteger(L, 2));
    const bool ok = winauto::CursorSet(x, y);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaEngine::L_PixelGet(lua_State* L) {
    const int x = static_cast<int>(luaL_checkinteger(L, 1));
    const int y = static_cast<int>(luaL_checkinteger(L, 2));
    uint8_t r = 0, g = 0, b = 0;
    if (!winauto::PixelGet(x, y, &r, &g, &b)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, r);
    lua_pushinteger(L, g);
    lua_pushinteger(L, b);
    return 3;
}

static bool ColorNear(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1, int tol) {
    tol = std::max(0, tol);
    return std::abs(static_cast<int>(r0) - static_cast<int>(r1)) <= tol &&
        std::abs(static_cast<int>(g0) - static_cast<int>(g1)) <= tol &&
        std::abs(static_cast<int>(b0) - static_cast<int>(b1)) <= tol;
}

int LuaEngine::L_ColorWait(lua_State* L) {
    auto* self = Self(L);
    const int x = static_cast<int>(luaL_checkinteger(L, 1));
    const int y = static_cast<int>(luaL_checkinteger(L, 2));
    const uint8_t rTarget = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    const uint8_t gTarget = static_cast<uint8_t>(luaL_checkinteger(L, 4));
    const uint8_t bTarget = static_cast<uint8_t>(luaL_checkinteger(L, 5));
    const int tol = (lua_gettop(L) >= 6) ? static_cast<int>(luaL_checkinteger(L, 6)) : 0;
    const int64_t timeoutMs = (lua_gettop(L) >= 7) ? static_cast<int64_t>(luaL_checkinteger(L, 7)) : 2000;
    const int64_t intervalMs = (lua_gettop(L) >= 8) ? static_cast<int64_t>(luaL_checkinteger(L, 8)) : 50;

    const int64_t deadline = timing::MicrosNow() + std::max<int64_t>(0, timeoutMs) * 1000;
    while (timing::MicrosNow() <= deadline) {
        uint8_t r = 0, g = 0, b = 0;
        if (winauto::PixelGet(x, y, &r, &g, &b) && ColorNear(r, g, b, rTarget, gTarget, bTarget, tol)) {
            lua_pushboolean(L, 1);
            return 1;
        }
        if (self) {
            self->WaitMicrosCancelable(std::max<int64_t>(0, intervalMs) * 1000);
            if (self->cancel_.load(std::memory_order_acquire)) luaL_error(L, "cancelled");
        } else {
            Sleep(static_cast<DWORD>(std::clamp<int64_t>(intervalMs, 0, 1000)));
        }
    }
    lua_pushboolean(L, 0);
    return 1;
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

    const bool horizontal = LuaBool01(L, 4, false);
    SendMouseWheelBestEffort(delta, horizontal);
    return 0;
}

static void SendKeyScancode(int scanCode, bool extended, bool down) {
    const HKL layout = GetKeyboardLayout(0);
    const UINT vk = MapVirtualKeyExW(static_cast<UINT>(scanCode), MAPVK_VSC_TO_VK_EX, layout);
    if (vk != 0) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = static_cast<WORD>(vk);
        in.ki.wScan = static_cast<WORD>(scanCode);
        if (extended) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        if (!down) in.ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(in));
        return;
    }

    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = 0;
    in.ki.wScan = static_cast<WORD>(scanCode);
    in.ki.dwFlags = KEYEVENTF_SCANCODE;
    if (extended) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    if (!down) in.ki.dwFlags |= KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(in));
}

static bool LuaBool01(lua_State* L, int idx, bool defaultValue) {
    if (!L) return defaultValue;
    const int t = lua_type(L, idx);
    if (t == LUA_TNONE || t == LUA_TNIL) return defaultValue;
    if (t == LUA_TBOOLEAN) return lua_toboolean(L, idx) != 0;
    if (t == LUA_TNUMBER) return lua_tointeger(L, idx) != 0;
    if (t == LUA_TSTRING) {
        const char* s = lua_tostring(L, idx);
        if (!s) return defaultValue;
        if (_stricmp(s, "0") == 0 || _stricmp(s, "false") == 0 || _stricmp(s, "no") == 0) return false;
        if (_stricmp(s, "1") == 0 || _stricmp(s, "true") == 0 || _stricmp(s, "yes") == 0) return true;
        return defaultValue;
    }
    return defaultValue;
}

static bool TryParseNamedVk(const char* s, uint32_t* vkOut, bool* extOut) {
    if (!s || !vkOut) return false;
    auto eq = [](const char* a, const char* b) { return _stricmp(a, b) == 0; };
    bool ext = false;
    uint32_t vk = 0;

    if (eq(s, "enter") || eq(s, "return")) vk = VK_RETURN;
    else if (eq(s, "tab")) vk = VK_TAB;
    else if (eq(s, "esc") || eq(s, "escape")) vk = VK_ESCAPE;
    else if (eq(s, "space")) vk = VK_SPACE;
    else if (eq(s, "backspace") || eq(s, "bs")) vk = VK_BACK;
    else if (eq(s, "delete") || eq(s, "del")) vk = VK_DELETE;
    else if (eq(s, "insert") || eq(s, "ins")) vk = VK_INSERT;
    else if (eq(s, "home")) vk = VK_HOME, ext = true;
    else if (eq(s, "end")) vk = VK_END, ext = true;
    else if (eq(s, "pageup") || eq(s, "pgup")) vk = VK_PRIOR, ext = true;
    else if (eq(s, "pagedown") || eq(s, "pgdn")) vk = VK_NEXT, ext = true;
    else if (eq(s, "left")) vk = VK_LEFT, ext = true;
    else if (eq(s, "right")) vk = VK_RIGHT, ext = true;
    else if (eq(s, "up")) vk = VK_UP, ext = true;
    else if (eq(s, "down")) vk = VK_DOWN, ext = true;
    else return false;

    *vkOut = vk;
    if (extOut) *extOut = ext;
    return true;
}

static bool TryVkFromLuaArg(lua_State* L, int idx, uint32_t* vkOut, uint8_t* modsOut, bool* extOut) {
    if (!L || !vkOut) return false;
    if (modsOut) *modsOut = 0;
    if (extOut) *extOut = false;

    const int t = lua_type(L, idx);
    if (t == LUA_TNUMBER) {
        *vkOut = static_cast<uint32_t>(lua_tointeger(L, idx));
        return true;
    }
    if (t != LUA_TSTRING) return false;

    const char* s = lua_tostring(L, idx);
    if (!s || s[0] == '\0') return false;

    uint32_t namedVk = 0;
    bool namedExt = false;
    if (TryParseNamedVk(s, &namedVk, &namedExt)) {
        *vkOut = namedVk;
        if (extOut) *extOut = namedExt;
        return true;
    }

    const int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (len <= 1) return false;
    std::wstring w;
    w.resize(static_cast<size_t>(len));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), len);

    if (w.size() != 2) return false;
    const wchar_t ch = w[0];

    const SHORT r = VkKeyScanW(ch);
    if (r == -1) return false;
    const uint8_t vk = static_cast<uint8_t>(r & 0xFF);
    const uint8_t state = static_cast<uint8_t>((r >> 8) & 0xFF);

    *vkOut = static_cast<uint32_t>(vk);
    if (modsOut) *modsOut = static_cast<uint8_t>(state & 0x07);
    return true;
}

static void SendVkModifiers(uint8_t mods, bool down) {
    if (mods & 0x02) SendKeyByScanOrVk(VK_CONTROL, 0, false, down);
    if (mods & 0x04) SendKeyByScanOrVk(VK_MENU, 0, false, down);
    if (mods & 0x01) SendKeyByScanOrVk(VK_SHIFT, 0, false, down);
}

int LuaEngine::L_KeyDown(lua_State* L) {
    const int scan = static_cast<int>(luaL_checkinteger(L, 1));
    const bool ext = LuaBool01(L, 2, false);
    SendKeyScancode(scan, ext, true);
    return 0;
}

int LuaEngine::L_KeyUp(lua_State* L) {
    const int scan = static_cast<int>(luaL_checkinteger(L, 1));
    const bool ext = LuaBool01(L, 2, false);
    SendKeyScancode(scan, ext, false);
    return 0;
}

int LuaEngine::L_VkDown(lua_State* L) {
    uint32_t vk = 0;
    bool inferredExt = false;
    if (!TryVkFromLuaArg(L, 1, &vk, nullptr, &inferredExt)) return 0;
    const bool ext = LuaBool01(L, 2, inferredExt);
    SendKeyByScanOrVk(vk, 0, ext, true);
    return 0;
}

int LuaEngine::L_VkUp(lua_State* L) {
    uint32_t vk = 0;
    bool inferredExt = false;
    if (!TryVkFromLuaArg(L, 1, &vk, nullptr, &inferredExt)) return 0;
    const bool ext = LuaBool01(L, 2, inferredExt);
    SendKeyByScanOrVk(vk, 0, ext, false);
    return 0;
}

int LuaEngine::L_VkPress(lua_State* L) {
    uint32_t vk = 0;
    uint8_t mods = 0;
    bool inferredExt = false;
    const bool mapped = TryVkFromLuaArg(L, 1, &vk, &mods, &inferredExt);
    const int64_t holdMs = (lua_gettop(L) >= 2) ? static_cast<int64_t>(luaL_checkinteger(L, 2)) : 60;
    const bool ext = LuaBool01(L, 3, inferredExt);

    if (!mapped) {
        if (lua_type(L, 1) == LUA_TSTRING) {
            const char* s = lua_tostring(L, 1);
            if (s && s[0] != '\0') {
                const int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
                if (len > 1) {
                    std::wstring w;
                    w.resize(static_cast<size_t>(len));
                    MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), len);
                    SendTextUtf16(w.c_str(), static_cast<int>(w.size() - 1));
                }
            }
        }
        return 0;
    }

    SendVkModifiers(mods, true);
    SendKeyByScanOrVk(vk, 0, ext, true);
    auto* self = Self(L);
    if (self) {
        self->WaitMicrosCancelable(std::max<int64_t>(0, holdMs) * 1000);
        if (self->cancel_.load(std::memory_order_acquire)) luaL_error(L, "cancelled");
    } else {
        timing::HighPrecisionWaitMicros(std::max<int64_t>(0, holdMs) * 1000);
    }
    SendKeyByScanOrVk(vk, 0, ext, false);
    SendVkModifiers(mods, false);
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
