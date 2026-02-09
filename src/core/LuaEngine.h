#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

struct lua_State;

class Replayer;

class LuaEngine {
public:
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

private:
    static int L_Playback(lua_State* L);
    static int L_HumanMove(lua_State* L);
    static int L_HumanClick(lua_State* L);
    static int L_HumanScroll(lua_State* L);
    static int L_SetSpeed(lua_State* L);
    static int L_WaitMs(lua_State* L);
    static int L_WaitUs(lua_State* L);
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
