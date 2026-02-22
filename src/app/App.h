#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>
#include <imgui.h>

#include "core/Hooks.h"
#include "core/Logger.h"
#include "core/LuaEngine.h"
#include "core/OverlayWindow.h"
#include "core/Recorder.h"
#include "core/Replayer.h"
#include "core/Scheduler.h"

struct LuaScriptUiState {
    bool docsOpen{ true };
    std::string docsFilter;
    int docsSelected{ -1 };

    bool assistEnabled{ true };
    bool completionOpen{ false };
    int completionCursorPos{ 0 };
    int completionWordStart{ 0 };
    int selectionStart{ 0 };
    int selectionEnd{ 0 };
    std::string completionPrefix;
    std::vector<int> completionMatches;
    int completionSelected{ 0 };
    std::string completionPendingInsert;
};

// Floating particle for background decoration
struct Particle {
    float x, y;
    float vx, vy;
    float radius;
    float alpha;
    float phase;       // for pulsing
};

class App {
public:
    App(HINSTANCE hInstance, HWND hwnd);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void OnFrame();
    void OnHotkey();
    void OnHotkeyStartResume();
    void OnHotkeyPause();
    void RequestExit();
    bool ShouldExit() const;

private:
    // UI drawing
    void DrawBackground();
    void DrawSimpleMode();
    void DrawAdvancedMode();
    void DrawSchedulerMode();
    void DrawLogMode();
    void DrawStatusBar();
    void DrawBlockInputConfirmModal();
    void DrawExitConfirmModal();
    void DrawAnimatedCursor(ImVec2 center, float radius, float time);
    void UpdateTaskbarIcon();

    // Scheduler
    void OnSchedulerTaskFired(const ScheduledTask& task);
    void SchedulerExecuteTask(const ScheduledTask& task);

    // Status helpers
    void SetStatusInfo(const std::string& text);
    void SetStatusOk(const std::string& text);
    void SetStatusWarn(const std::string& text);
    void SetStatusError(const std::string& text);

    // Recording / Replay
    void StartRecording();
    void StopRecording();
    void StartReplay();
    void StartReplayConfirmed();
    void StopReplay();
    void EmergencyStop();

    // File dialogs
    static bool OpenFileDialog(HWND owner, wchar_t* path, uint32_t pathCapacity, const wchar_t* filter);
    static bool SaveFileDialog(HWND owner, wchar_t* path, uint32_t pathCapacity, const wchar_t* filter);

    // File I/O
    static std::string ReadTextFile(const std::wstring& filename);
    static bool WriteTextFile(const std::wstring& filename, const std::string& content);

    // Config save/load
    void LoadConfig();
    void SaveConfig();

    HINSTANCE hInstance_{};
    HWND hwnd_{};

    // Window geometry (persisted)
    int savedWinX_{ -1 };
    int savedWinY_{ -1 };
    int savedWinW_{ 0 };
    int savedWinH_{ 0 };
    bool savedWinMaximized_{ false };

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

    bool exitConfirmOpen_{ false };
    bool exitConfirmed_{ false };

    int luaLastHighlightLine_{ 0 };

    bool minimizeOnScriptRun_{ true };
    bool scriptMinimized_{ false };
    LuaScriptUiState luaUi_{};

    // Animation state
    float animTime_{ 0.0f };
    std::vector<Particle> particles_;
    bool particlesInited_{ false };

    // Taskbar icon animation
    float lastIconUpdateTime_{ -1.0f };
    HICON taskbarIcon_{ nullptr };

    // Scheduler
    Scheduler scheduler_;

    // Scheduler UI state
    ScheduledTask editTask_{};
    int schedSelectedTask_{ -1 };
    int schedDetailTab_{ 0 };       // 0=info, 1=history
    bool schedEditingExisting_{ false }; // true = editing selected task, false = new task
    bool schedFormExpanded_{ true };     // top form collapsed/expanded
    int  schedDeleteConfirmId_{ -1 };    // task id pending delete confirmation
    std::string schedValidationMsg_;     // inline validation error message

    // Splitter ratios (persisted)
    float simpleCol1Ratio_{ 0.30f };  // 录制回放: left column
    float simpleCol2Ratio_{ 0.35f };  // 录制回放: middle column
    float schedCol1Ratio_{ 0.50f };   // 定时任务: shared ratio for top & bottom

    // Log UI state
    int logFilterLevel_{ 1 };       // default: INFO
    bool logAutoScroll_{ true };
    bool logFileOutput_{ false };
    std::string logFilePath_{ "autoclicker.log" };
    int logMaxEntries_{ 10000 };

public:
    // Screen rect of the scrollable editor area (set each frame by DrawLuaEditorWithLineNumbers)
    // WndProc uses this to decide whether to pass WM_MOUSEWHEEL to ImGui.
    RECT editorScreenRect_{ 0, 0, 0, 0 };
    bool editorRectValid_{ false };

    void ApplySavedWindowGeometry();
    void SaveWindowGeometry();
};
