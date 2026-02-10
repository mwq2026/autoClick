#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct lua_State;

class Replayer;

class LuaEngine {
public:
    struct LuaApiDoc {
        const char* name;
        const char* signature;
        const char* group;
        const char* brief;
    };

    LuaEngine();
    ~LuaEngine();

    LuaEngine(const LuaEngine&) = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

    bool Init(Replayer* replayer);
    void Shutdown();

    bool RunString(const std::string& code, std::string* errorOut);
    bool RunFile(const std::wstring& filename, std::string* errorOut);

    bool StartAsync(const std::string& code);
    void StopAsync();
    bool IsRunning() const;
    int CurrentLine() const;
    std::string LastError() const;
    static const std::vector<LuaApiDoc>& ApiDocs();

private:
    static int L_Playback(lua_State* L);
    static int L_HumanMove(lua_State* L);
    static int L_HumanClick(lua_State* L);
    static int L_HumanScroll(lua_State* L);
    static int L_SetSpeed(lua_State* L);
    static int L_WaitMs(lua_State* L);
    static int L_WaitUs(lua_State* L);
    static int L_ActivateWindow(lua_State* L);
    static int L_WindowIsValid(lua_State* L);
    static int L_WindowFromPoint(lua_State* L);
    static int L_WindowForeground(lua_State* L);
    static int L_WindowFind(lua_State* L);
    static int L_WindowFindAll(lua_State* L);
    static int L_WindowWait(lua_State* L);
    static int L_WindowTitle(lua_State* L);
    static int L_WindowClass(lua_State* L);
    static int L_WindowPid(lua_State* L);
    static int L_WindowRect(lua_State* L);
    static int L_WindowClientRect(lua_State* L);
    static int L_WindowActivate(lua_State* L);
    static int L_WindowActivateAt(lua_State* L);
    static int L_WindowSetTopmost(lua_State* L);
    static int L_WindowBringToTop(lua_State* L);
    static int L_WindowSendToBack(lua_State* L);
    static int L_WindowShow(lua_State* L);
    static int L_WindowHide(lua_State* L);
    static int L_WindowMinimize(lua_State* L);
    static int L_WindowMaximize(lua_State* L);
    static int L_WindowRestore(lua_State* L);
    static int L_WindowMove(lua_State* L);
    static int L_WindowResize(lua_State* L);
    static int L_WindowSetRect(lua_State* L);
    static int L_WindowClose(lua_State* L);
    static int L_WindowCloseForce(lua_State* L);
    static int L_ProcessStart(lua_State* L);
    static int L_ProcessIsRunning(lua_State* L);
    static int L_ProcessWait(lua_State* L);
    static int L_ProcessKill(lua_State* L);
    static int L_ClipboardSet(lua_State* L);
    static int L_ClipboardGet(lua_State* L);
    static int L_ScreenSize(lua_State* L);
    static int L_CursorPos(lua_State* L);
    static int L_CursorSet(lua_State* L);
    static int L_PixelGet(lua_State* L);
    static int L_ColorWait(lua_State* L);
    static int L_MouseMove(lua_State* L);
    static int L_MouseDown(lua_State* L);
    static int L_MouseUp(lua_State* L);
    static int L_MouseWheel(lua_State* L);
    static int L_KeyDown(lua_State* L);
    static int L_KeyUp(lua_State* L);
    static int L_VkDown(lua_State* L);
    static int L_VkUp(lua_State* L);
    static int L_VkPress(lua_State* L);
    static int L_Text(lua_State* L);
    static int L_SetTargetWindow(lua_State* L);
    static int L_ClearTargetWindow(lua_State* L);

    static LuaEngine* Self(lua_State* L);
    static void RegisterApi(lua_State* L);
    static void DebugHook(lua_State* L, struct lua_Debug* ar);
    void WaitMicrosCancelable(int64_t us);

    lua_State* L_{ nullptr };
    Replayer* replayer_{ nullptr };

    std::atomic<bool> running_{ false };
    std::atomic<bool> cancel_{ false };
    std::atomic<int> currentLine_{ 0 };
    mutable std::mutex errorMutex_;
    std::string lastError_;
    std::thread worker_;

    bool hasLastMouse_{ false };
    int lastMouseX_{ 0 };
    int lastMouseY_{ 0 };
    void* targetWindow_{ nullptr };
};
