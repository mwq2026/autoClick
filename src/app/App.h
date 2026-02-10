#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

#include "core/Hooks.h"
#include "core/LuaEngine.h"
#include "core/OverlayWindow.h"
#include "core/Recorder.h"
#include "core/Replayer.h"

struct LuaScriptUiState {
    bool docsOpen{ true };
    std::string docsFilter;
    int docsSelected{ -1 };

    bool assistEnabled{ true };
    bool completionOpen{ false };
    int completionCursorPos{ 0 };
    int completionWordStart{ 0 };
    std::string completionPrefix;
    std::vector<int> completionMatches;
    int completionSelected{ 0 };
    std::string completionPendingInsert;
};

class App {
public:
    App(HINSTANCE hInstance, HWND hwnd);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void OnFrame();
    void OnHotkey();

private:
    void DrawSimpleMode();
    void DrawAdvancedMode();
    void DrawStatusBar();
    void DrawBlockInputConfirmModal();
    void SetStatusInfo(const std::string& text);
    void SetStatusOk(const std::string& text);
    void SetStatusWarn(const std::string& text);
    void SetStatusError(const std::string& text);

    void StartRecording();
    void StopRecording();
    void StartReplay();
    void StartReplayConfirmed();
    void StopReplay();
    void EmergencyStop();

    static bool OpenFileDialog(HWND owner, wchar_t* path, uint32_t pathCapacity, const wchar_t* filter);
    static bool SaveFileDialog(HWND owner, wchar_t* path, uint32_t pathCapacity, const wchar_t* filter);

    static std::string ReadTextFile(const std::wstring& filename);
    static bool WriteTextFile(const std::wstring& filename, const std::string& content);

    HINSTANCE hInstance_{};
    HWND hwnd_{};

    Recorder recorder_;
    Hooks hooks_;
    Replayer replayer_;
    LuaEngine lua_;
    OverlayWindow overlay_;

    int64_t recordStartQpc_{ 0 };

    bool blockInput_{ false };
    float speedFactor_{ 1.0f };

    int mode_{ 0 };

    std::string trcPath_{ "task.trc" };
    std::string luaPath_{ "task.lua" };

    std::string luaEditor_;
    std::string luaLastError_;

    bool exportFull_{ true };

    int statusLevel_{ 0 };
    std::string statusText_;
    int64_t statusExpireMicros_{ 0 };

    bool blockInputConfirmOpen_{ false };
    bool pendingStartReplay_{ false };
    int lastBlockInputState_{ 0 };
    bool blockInputUnderstood_{ false };

    int luaLastHighlightLine_{ 0 };

    bool minimizeOnScriptRun_{ true };
    bool scriptMinimized_{ false };
    LuaScriptUiState luaUi_{};
};
