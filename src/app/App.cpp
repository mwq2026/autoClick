#include "app/App.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <commdlg.h>
#include <imgui.h>

#include "core/Converter.h"
#include "core/HighResClock.h"

static bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags extraFlags);

static float UiScale() {
    const float fontSize = ImGui::GetFontSize();
    return fontSize > 0.0f ? (fontSize / 18.0f) : 1.0f;
}

static bool IsCurrentProcessElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elev{};
    DWORD cb = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &cb) != FALSE;
    CloseHandle(token);
    return ok ? (elev.TokenIsElevated != 0) : false;
}

static bool IsWindowProcessElevated(HWND hwnd) {
    if (!hwnd) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    HANDLE token = nullptr;
    if (!OpenProcessToken(h, TOKEN_QUERY, &token)) {
        CloseHandle(h);
        return false;
    }
    TOKEN_ELEVATION elev{};
    DWORD cb = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &cb) != FALSE;
    CloseHandle(token);
    CloseHandle(h);
    return ok ? (elev.TokenIsElevated != 0) : false;
}

static HWND RootWindowAtCursor() {
    POINT pt{};
    if (!GetCursorPos(&pt)) return nullptr;
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd) return nullptr;
    return GetAncestor(hwnd, GA_ROOT);
}

static void DrawAppLogo(ImVec2 size) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = std::min(size.x, size.y);
    const float r = s * 0.5f;
    const ImVec2 c = ImVec2(p.x + r, p.y + r);

    dl->AddCircleFilled(c, r, IM_COL32(38, 45, 56, 255));
    dl->AddCircleFilled(c, r * 0.86f, IM_COL32(35, 161, 220, 255));

    const ImVec2 a = ImVec2(c.x - r * 0.18f, c.y - r * 0.28f);
    const ImVec2 b = ImVec2(c.x - r * 0.18f, c.y + r * 0.28f);
    const ImVec2 d = ImVec2(c.x + r * 0.36f, c.y);
    dl->AddTriangleFilled(a, b, d, IM_COL32(255, 255, 255, 235));

    ImGui::Dummy(ImVec2(s, s));
}

static void DrawLuaEditorWithLineNumbers(std::string* text, float height, bool readOnly, int highlightLine, int* lastScrollToLine) {
    if (!text) return;

    const float s = UiScale();
    const float gutter = 70.0f * s;
    ImGui::BeginChild("##lua_editor_child", ImVec2(-1, height), true, ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float lineH = ImGui::GetTextLineHeightWithSpacing();

    int lineCount = 1;
    for (char ch : *text) if (ch == '\n') ++lineCount;

    const float desiredEditorH = std::max(height, lineCount * lineH + 8.0f * s);
    ImGui::SetCursorPosX(gutter);
    InputTextMultilineString("##luaeditor", text, ImVec2(-1, desiredEditorH), readOnly ? ImGuiInputTextFlags_ReadOnly : 0);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 winPos = ImGui::GetWindowPos();
    const ImVec2 winSize = ImGui::GetWindowSize();
    dl->PushClipRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), true);

    if (highlightLine > 0) {
        const int i = highlightLine - 1;
        const float y = origin.y + i * lineH;
        dl->AddRectFilled(ImVec2(winPos.x, y), ImVec2(winPos.x + winSize.x, y + lineH), IM_COL32(70, 120, 255, 70));
    }

    const float scrollY = ImGui::GetScrollY();
    const int first = std::max(0, static_cast<int>(scrollY / lineH));
    const int visible = static_cast<int>(winSize.y / lineH) + 3;
    const int last = std::min(lineCount, first + visible);

    char buf[16]{};
    for (int i = first; i < last; ++i) {
        std::snprintf(buf, sizeof(buf), "%05d", i + 1);
        const float y = origin.y + i * lineH;
        dl->AddText(ImVec2(origin.x + 6.0f * s, y), IM_COL32(190, 190, 190, 255), buf);
    }

    dl->PopClipRect();

    if (highlightLine > 0 && lastScrollToLine && *lastScrollToLine != highlightLine) {
        const float targetY = (highlightLine - 1) * lineH;
        ImGui::SetScrollFromPosY(origin.y + targetY, 0.35f);
        *lastScrollToLine = highlightLine;
    }

    ImGui::EndChild();
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out;
    out.resize(static_cast<size_t>(len));
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

static std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out;
    out.resize(static_cast<size_t>(len));
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
    return out;
}

static int ImGuiStringResizeCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) return 0;
    auto* str = static_cast<std::string*>(data->UserData);
    str->resize(static_cast<size_t>(data->BufTextLen));
    data->Buf = str->data();
    return 0;
}

static bool InputTextString(const char* label, std::string* str, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    if (str->capacity() < 256) str->reserve(256);
    str->resize(str->capacity());
    const bool changed = ImGui::InputText(label, str->data(), str->size() + 1, flags, ImGuiStringResizeCallback, str);
    str->resize(std::strlen(str->c_str()));
    return changed;
}

static bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags extraFlags = 0) {
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize | extraFlags;
    if (str->capacity() < 4096) str->reserve(4096);
    str->resize(str->capacity());
    const bool changed = ImGui::InputTextMultiline(label, str->data(), str->size() + 1, size, flags, ImGuiStringResizeCallback, str);
    str->resize(std::strlen(str->c_str()));
    return changed;
}

App::App(HINSTANCE hInstance, HWND hwnd) : hInstance_(hInstance), hwnd_(hwnd) {
    overlay_.Create(hInstance_);
    lua_.Init(&replayer_);
    luaEditor_ =
        "set_speed(1.0)\n"
        "mouse_move(500, 500)\n"
        "mouse_down('left')\n"
        "wait_ms(60)\n"
        "mouse_up('left')\n";
}

App::~App() {
    EmergencyStop();
    lua_.Shutdown();
    overlay_.Destroy();
}

void App::OnHotkey() {
    EmergencyStop();
}

void App::OnFrame() {
    const float s = UiScale();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f * s;
    style.FrameRounding = 6.0f * s;
    style.GrabRounding = 6.0f * s;
    style.ScrollbarRounding = 8.0f * s;

    const int blockState = replayer_.BlockInputState();
    if (blockState != lastBlockInputState_) {
        if (blockState == 1) SetStatusWarn("已启用屏蔽系统输入，Ctrl+F12 可紧急停止");
        if (blockState == -1) SetStatusError("屏蔽系统输入失败（可能需要管理员权限）");
        if (lastBlockInputState_ == 1 && blockState == 0) SetStatusOk("已恢复系统输入");
        lastBlockInputState_ = blockState;
    }

    if (recorder_.IsRecording()) {
        const int64_t elapsed = timing::QpcDeltaToMicros(timing::QpcNow() - recordStartQpc_);
        overlay_.SetElapsedMicros(elapsed);
        if (!overlay_.IsVisible()) overlay_.Show();
    } else {
        if (overlay_.IsVisible()) overlay_.Hide();
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
    ImGui::Begin(
        "AutoClicker-Pro",
        nullptr,
        ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings);

    DrawAppLogo(ImVec2(20.0f * s, 20.0f * s));
    ImGui::SameLine();
    ImGui::TextUnformatted("Ctrl+F12: Emergency Stop");

    ImGui::RadioButton("简单模式", &mode_, 0);
    ImGui::SameLine();
    ImGui::RadioButton("高级模式 (Lua)", &mode_, 1);
    ImGui::Separator();

    if (mode_ == 0) DrawSimpleMode();
    if (mode_ == 1) DrawAdvancedMode();

    DrawStatusBar();
    DrawBlockInputConfirmModal();
    ImGui::End();

    if (!lua_.IsRunning() && scriptMinimized_ && hwnd_) {
        ShowWindow(hwnd_, SW_RESTORE);
        SetForegroundWindow(hwnd_);
        scriptMinimized_ = false;
    }
}

void App::DrawStatusBar() {
    const int64_t now = timing::MicrosNow();
    if (statusText_.empty() || now >= statusExpireMicros_) return;

    ImVec4 c = ImVec4(0.78f, 0.78f, 0.78f, 1.0f);
    if (statusLevel_ == 1) c = ImVec4(0.30f, 0.85f, 0.45f, 1.0f);
    if (statusLevel_ == 2) c = ImVec4(0.98f, 0.76f, 0.30f, 1.0f);
    if (statusLevel_ == 3) c = ImVec4(1.00f, 0.35f, 0.35f, 1.0f);

    ImGui::Separator();
    ImGui::TextColored(c, "%s", statusText_.c_str());
}

void App::DrawBlockInputConfirmModal() {
    if (blockInputConfirmOpen_) {
        blockInputUnderstood_ = false;
        ImGui::OpenPopup("确认屏蔽系统输入");
        blockInputConfirmOpen_ = false;
    }

    bool open = true;
    if (ImGui::BeginPopupModal("确认屏蔽系统输入", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        const float s = UiScale();
        ImGui::TextUnformatted("你即将启用屏蔽系统输入（BlockInput）。");
        ImGui::TextUnformatted("启用后鼠标键盘可能暂时不可用。");
        ImGui::TextUnformatted("紧急停止：Ctrl+F12");
        ImGui::Separator();
        ImGui::Checkbox("我已理解风险", &blockInputUnderstood_);
        ImGui::Separator();

        const bool canContinue = blockInputUnderstood_;
        if (!canContinue) ImGui::BeginDisabled();
        if (ImGui::Button("继续启用并回放", ImVec2(180.0f * s, 0))) {
            pendingStartReplay_ = false;
            ImGui::CloseCurrentPopup();
            StartReplayConfirmed();
        }
        if (!canContinue) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(120.0f * s, 0))) {
            pendingStartReplay_ = false;
            ImGui::CloseCurrentPopup();
            SetStatusInfo("已取消回放");
        }
        ImGui::EndPopup();
    }
}

void App::SetStatusInfo(const std::string& text) {
    statusLevel_ = 0;
    statusText_ = text;
    statusExpireMicros_ = timing::MicrosNow() + 3'000'000;
}

void App::SetStatusOk(const std::string& text) {
    statusLevel_ = 1;
    statusText_ = text;
    statusExpireMicros_ = timing::MicrosNow() + 3'000'000;
}

void App::SetStatusWarn(const std::string& text) {
    statusLevel_ = 2;
    statusText_ = text;
    statusExpireMicros_ = timing::MicrosNow() + 6'000'000;
}

void App::SetStatusError(const std::string& text) {
    statusLevel_ = 3;
    statusText_ = text;
    statusExpireMicros_ = timing::MicrosNow() + 8'000'000;
}

void App::DrawSimpleMode() {
    const float s = UiScale();
    const bool recording = recorder_.IsRecording();
    const bool replaying = replayer_.IsRunning();

    ImGui::TextUnformatted("任务文件 (.trc)");
    InputTextString("##trcpath", &trcPath_);
    ImGui::SameLine();
    if (ImGui::Button("浏览##trc")) {
        wchar_t buf[MAX_PATH]{};
        std::wstring w = Utf8ToWide(trcPath_);
        wcsncpy_s(buf, w.c_str(), _TRUNCATE);
        if (OpenFileDialog(nullptr, buf, MAX_PATH, L"Tiny Record (*.trc)\0*.trc\0\0")) trcPath_ = WideToUtf8(std::wstring(buf));
    }
    ImGui::SameLine();
    if (ImGui::Button("另存为##trc")) {
        wchar_t buf[MAX_PATH]{};
        std::wstring w = Utf8ToWide(trcPath_);
        wcsncpy_s(buf, w.c_str(), _TRUNCATE);
        if (SaveFileDialog(nullptr, buf, MAX_PATH, L"Tiny Record (*.trc)\0*.trc\0\0")) trcPath_ = WideToUtf8(std::wstring(buf));
    }

    ImGui::Checkbox("屏蔽系统输入 (可能需要管理员权限)", &blockInput_);
    ImGui::SliderFloat("回放倍速", &speedFactor_, 0.5f, 10.0f, "%.2f");
    replayer_.SetSpeed(speedFactor_);

    ImGui::Separator();

    if (!recording && !replaying) {
        if (ImGui::Button("开始录制", ImVec2(140.0f * s, 0))) StartRecording();
        ImGui::SameLine();
        if (ImGui::Button("加载 .trc", ImVec2(140.0f * s, 0))) {
            if (recorder_.LoadFromFile(Utf8ToWide(trcPath_))) SetStatusOk("加载成功");
            else SetStatusError("加载失败：无法读取 .trc");
        }
    } else if (recording) {
        if (ImGui::Button("停止录制", ImVec2(140.0f * s, 0))) StopRecording();
        ImGui::SameLine();
        if (ImGui::Button("保存 .trc", ImVec2(140.0f * s, 0))) {
            if (recorder_.SaveToFile(Utf8ToWide(trcPath_))) SetStatusOk("保存成功");
            else SetStatusError("保存失败：无法写入 .trc");
        }
    } else if (replaying) {
        if (ImGui::Button("停止回放", ImVec2(140.0f * s, 0))) StopReplay();
    }

    if (!recording && !replaying) {
        ImGui::SameLine();
        if (ImGui::Button("开始回放", ImVec2(140.0f * s, 0))) StartReplay();
    }

    ImGui::Separator();

    const auto& events = recorder_.Events();
    int cntMove = 0;
    int cntDown = 0;
    int cntUp = 0;
    int cntWheel = 0;
    int cntKeyDown = 0;
    int cntKeyUp = 0;
    for (const auto& e : events) {
        switch (static_cast<trc::EventType>(e.type)) {
        case trc::EventType::MouseMove: ++cntMove; break;
        case trc::EventType::MouseDown: ++cntDown; break;
        case trc::EventType::MouseUp: ++cntUp; break;
        case trc::EventType::Wheel: ++cntWheel; break;
        case trc::EventType::KeyDown: ++cntKeyDown; break;
        case trc::EventType::KeyUp: ++cntKeyUp; break;
        default: break;
        }
    }

    ImGui::Text("事件数: %d", static_cast<int>(events.size()));
    ImGui::Text("总时长: %.3fs", static_cast<double>(recorder_.TotalDurationMicros()) / 1'000'000.0);
    ImGui::Text("Move: %d  Down: %d  Up: %d  Wheel: %d  KeyDown: %d  KeyUp: %d", cntMove, cntDown, cntUp, cntWheel, cntKeyDown, cntKeyUp);

    if (replaying) {
        ImGui::TextUnformatted("回放进度");
        ImGui::ProgressBar(replayer_.Progress01(), ImVec2(-1, 0));
    }
}

void App::DrawAdvancedMode() {
    const float s = UiScale();
    ImGui::TextUnformatted("Lua 文件 (.lua)");
    InputTextString("##luapath", &luaPath_);
    ImGui::SameLine();
    if (ImGui::Button("浏览##lua")) {
        wchar_t buf[MAX_PATH]{};
        std::wstring w = Utf8ToWide(luaPath_);
        wcsncpy_s(buf, w.c_str(), _TRUNCATE);
        if (OpenFileDialog(nullptr, buf, MAX_PATH, L"Lua Script (*.lua)\0*.lua\0\0")) luaPath_ = WideToUtf8(std::wstring(buf));
    }

    if (ImGui::Button("打开到编辑器")) {
        luaEditor_ = ReadTextFile(Utf8ToWide(luaPath_));
        luaLastError_.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("保存编辑器内容")) {
        WriteTextFile(Utf8ToWide(luaPath_), luaEditor_);
    }
    ImGui::SameLine();
    const bool scriptRunning = lua_.IsRunning();
    if (!scriptRunning) {
        if (ImGui::Button("运行脚本")) {
            luaLastError_.clear();
            if (!lua_.StartAsync(luaEditor_)) {
                SetStatusError("脚本启动失败");
            } else {
                HWND target = RootWindowAtCursor();
                if (target) {
                    const bool targetElev = IsWindowProcessElevated(target);
                    const bool selfElev = IsCurrentProcessElevated();
                    if (targetElev && !selfElev) {
                        SetStatusWarn("目标窗口是管理员权限，当前程序非管理员，键盘/滚轮可能被系统拦截");
                    }
                }

                if (minimizeOnScriptRun_ && hwnd_) {
                    ShowWindow(hwnd_, SW_MINIMIZE);
                    scriptMinimized_ = true;
                }

                luaLastHighlightLine_ = 0;
                SetStatusOk("脚本开始执行");
            }
        }
    } else {
        if (ImGui::Button("停止脚本")) {
            lua_.StopAsync();
            SetStatusInfo("已停止脚本");
        }
    }

    ImGui::SameLine();
    ImGui::Checkbox("脚本运行时最小化窗口", &minimizeOnScriptRun_);

    if (!luaLastError_.empty()) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", luaLastError_.c_str());
    }

    const int curLine = scriptRunning ? lua_.CurrentLine() : 0;
    if (scriptRunning) {
        if (curLine > 0) ImGui::Text("当前执行行: %d", curLine);
        else ImGui::TextUnformatted("当前执行行: -");
    }
    DrawLuaEditorWithLineNumbers(&luaEditor_, 260.0f * s, scriptRunning, curLine, &luaLastHighlightLine_);

    const std::string err = lua_.LastError();
    if (!err.empty() && luaLastError_ != err) {
        luaLastError_ = err;
        SetStatusError("脚本运行失败");
    }

    ImGui::Separator();

    static float tol = 3.0f;
    ImGui::SliderFloat("路径关键点容差(px)", &tol, 0.5f, 20.0f, "%.1f");
    ImGui::Checkbox("高保真导出（包含点击/滚轮/按键）", &exportFull_);
    if (ImGui::Button("从 .trc 导出到 .lua")) {
        bool ok = false;
        if (exportFull_) ok = Converter::TrcToLuaFull(Utf8ToWide(trcPath_), Utf8ToWide(luaPath_));
        else ok = Converter::TrcToLua(Utf8ToWide(trcPath_), Utf8ToWide(luaPath_), tol);
        if (ok) {
            luaEditor_ = ReadTextFile(Utf8ToWide(luaPath_));
            luaLastError_.clear();
            SetStatusOk("导出成功");
        } else {
            SetStatusError("导出失败：无法读取 .trc 或写入 .lua");
        }
    }
}

void App::StartRecording() {
    EmergencyStop();
    recorder_.Start();
    hooks_.Install(&recorder_);
    recordStartQpc_ = timing::QpcNow();
    overlay_.SetRecording(true);
    overlay_.SetElapsedMicros(0);
    overlay_.Show();
}

void App::StopRecording() {
    hooks_.Uninstall();
    recorder_.Stop();
    overlay_.SetRecording(false);
    overlay_.Hide();
}

void App::StartReplay() {
    if (blockInput_) {
        pendingStartReplay_ = true;
        blockInputConfirmOpen_ = true;
        return;
    }
    StartReplayConfirmed();
}

void App::StartReplayConfirmed() {
    if (recorder_.IsRecording()) StopRecording();
    if (!recorder_.LoadFromFile(Utf8ToWide(trcPath_))) {
        SetStatusError("回放失败：无法读取 .trc");
        return;
    }
    const auto& ev = recorder_.Events();
    std::vector<trc::RawEvent> copy(ev.begin(), ev.end());
    replayer_.SetSpeed(speedFactor_);
    if (replayer_.Start(std::move(copy), blockInput_, speedFactor_)) SetStatusOk("已开始回放");
    else SetStatusError("回放失败：无法启动回放线程");
}

void App::StopReplay() {
    replayer_.Stop();
    SetStatusInfo("已停止回放");
}

void App::EmergencyStop() {
    lua_.StopAsync();
    hooks_.Uninstall();
    recorder_.Stop();
    replayer_.Stop();
    overlay_.SetRecording(false);
    overlay_.Hide();
    SetStatusOk("已紧急停止");
}

bool App::OpenFileDialog(HWND owner, wchar_t* path, uint32_t pathCapacity, const wchar_t* filter) {
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = path;
    ofn.nMaxFile = pathCapacity;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
    return GetOpenFileNameW(&ofn) != FALSE;
}

bool App::SaveFileDialog(HWND owner, wchar_t* path, uint32_t pathCapacity, const wchar_t* filter) {
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = path;
    ofn.nMaxFile = pathCapacity;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
    return GetSaveFileNameW(&ofn) != FALSE;
}

std::string App::ReadTextFile(const std::wstring& filename) {
    std::ifstream in(std::filesystem::path(filename), std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool App::WriteTextFile(const std::wstring& filename, const std::string& content) {
    std::ofstream out(std::filesystem::path(filename), std::ios::binary);
    if (!out) return false;
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
}
