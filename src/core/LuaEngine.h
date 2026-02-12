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

    // Spy++ / UI Automation extensions
    static int L_WindowParent(lua_State* L);
    static int L_WindowOwner(lua_State* L);
    static int L_WindowChildFirst(lua_State* L);
    static int L_WindowNextSibling(lua_State* L);
    static int L_WindowPrevSibling(lua_State* L);
    static int L_WindowChildren(lua_State* L);
    static int L_WindowDesktop(lua_State* L);
    static int L_WindowStyle(lua_State* L);
    static int L_WindowExStyle(lua_State* L);
    static int L_WindowSetStyleLua(lua_State* L);
    static int L_WindowSetExStyleLua(lua_State* L);
    static int L_WindowIsVisibleLua(lua_State* L);
    static int L_WindowIsEnabledLua(lua_State* L);
    static int L_WindowIsFocusedLua(lua_State* L);
    static int L_WindowIsMinimizedLua(lua_State* L);
    static int L_WindowIsMaximizedLua(lua_State* L);
    static int L_WindowThreadIdLua(lua_State* L);
    static int L_WindowTextLength(lua_State* L);
    static int L_ControlGetText(lua_State* L);
    static int L_ControlSetText(lua_State* L);
    static int L_WindowEnableLua(lua_State* L);
    static int L_WindowSetFocusLua(lua_State* L);
    static int L_WindowSendMsg(lua_State* L);
    static int L_WindowPostMsg(lua_State* L);
    static int L_ButtonClickLua(lua_State* L);
    static int L_CheckboxGet(lua_State* L);
    static int L_CheckboxSet(lua_State* L);
    static int L_ComboGetSel(lua_State* L);
    static int L_ComboSetSel(lua_State* L);
    static int L_ComboGetCount(lua_State* L);
    static int L_ComboGetItem(lua_State* L);
    static int L_ListboxGetSel(lua_State* L);
    static int L_ListboxSetSel(lua_State* L);
    static int L_ListboxGetCountLua(lua_State* L);
    static int L_ListboxGetItemLua(lua_State* L);
    static int L_EditGetLineCount(lua_State* L);
    static int L_EditGetLine(lua_State* L);
    static int L_EditSetSel(lua_State* L);
    static int L_EditReplaceSel(lua_State* L);
    static int L_EditGetSel(lua_State* L);
    static int L_ScrollSet(lua_State* L);
    static int L_ScrollGetPos(lua_State* L);
    static int L_ScrollGetRange(lua_State* L);
    static int L_TabGetSel(lua_State* L);
    static int L_TabSetSel(lua_State* L);
    static int L_TabGetCountLua(lua_State* L);
    static int L_TreeViewGetCountLua(lua_State* L);
    static int L_TreeViewGetSelLua(lua_State* L);
    static int L_TreeViewSelectLua(lua_State* L);
    static int L_ListViewGetCountLua(lua_State* L);
    static int L_ListViewGetSelCountLua(lua_State* L);
    static int L_ListViewNextSelLua(lua_State* L);
    static int L_FindChildByClassLua(lua_State* L);
    static int L_FindChildByTextLua(lua_State* L);
    static int L_ScreenCapture(lua_State* L);
    static int L_MonitorCount(lua_State* L);
    static int L_MonitorRect(lua_State* L);
    static int L_SystemDpi(lua_State* L);
    static int L_WindowDpiLua(lua_State* L);
    static int L_RegRead(lua_State* L);
    static int L_RegWrite(lua_State* L);
    static int L_RegReadDwordLua(lua_State* L);
    static int L_RegWriteDwordLua(lua_State* L);
    static int L_EnvGetLua(lua_State* L);
    static int L_EnvSetLua(lua_State* L);
    static int L_FileExistsLua(lua_State* L);
    static int L_DirExistsLua(lua_State* L);
    static int L_FileDeleteLua(lua_State* L);
    static int L_DirCreateLua(lua_State* L);
    static int L_FileSizeLua(lua_State* L);
    static int L_MsgBoxLua(lua_State* L);
    static int L_Sleep(lua_State* L);

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
