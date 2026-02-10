#include "app/App.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cfloat>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

#include <commdlg.h>
#include <imgui.h>

#include "core/Converter.h"
#include "core/HighResClock.h"

static bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags extraFlags);
static bool InputTextMultilineStringWithCallback(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags extraFlags, ImGuiInputTextCallback callback, void* userData);
static int LuaEditorInputCallback(ImGuiInputTextCallbackData* data);

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

    // 浅色主题适配的 Logo
    dl->AddCircleFilled(c, r, IM_COL32(240, 240, 245, 255));  // 浅灰外圈
    dl->AddCircleFilled(c, r * 0.86f, IM_COL32(52, 143, 219, 255));  // 蓝色主体

    const ImVec2 a = ImVec2(c.x - r * 0.18f, c.y - r * 0.28f);
    const ImVec2 b = ImVec2(c.x - r * 0.18f, c.y + r * 0.28f);
    const ImVec2 d = ImVec2(c.x + r * 0.36f, c.y);
    dl->AddTriangleFilled(a, b, d, IM_COL32(255, 255, 255, 245));

    ImGui::Dummy(ImVec2(s, s));
}

static void DrawLuaEditorWithLineNumbers(LuaScriptUiState* ui, std::string* text, float height, bool readOnly, int highlightLine, int* lastScrollToLine) {
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
    const ImGuiInputTextFlags ro = readOnly ? ImGuiInputTextFlags_ReadOnly : 0;
    const ImGuiInputTextFlags assistFlags = ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCompletion;
    if (!readOnly && ui && ui->assistEnabled) {
        InputTextMultilineStringWithCallback("##luaeditor", text, ImVec2(-1, desiredEditorH), ro | assistFlags, LuaEditorInputCallback, ui);
    } else {
        InputTextMultilineString("##luaeditor", text, ImVec2(-1, desiredEditorH), ro);
    }
    const bool editorActive = ImGui::IsItemActive();
    if (ui && !readOnly && ui->assistEnabled && editorActive) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space)) {
            ui->completionOpen = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ui->completionOpen = false;
        }
    }

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

    if (ui && ui->assistEnabled) {
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 fp = ImGui::GetStyle().FramePadding;
        const ImVec2 textBase(itemMin.x + fp.x, itemMin.y + fp.y);

        const auto& docs = LuaEngine::ApiDocs();
        auto isIdent = [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
        };
        auto isApi = [&docs](std::string_view tok) {
            for (const auto& d : docs) {
                if (!d.name) continue;
                const size_t n = std::strlen(d.name);
                if (n == tok.size() && std::memcmp(d.name, tok.data(), n) == 0) return true;
            }
            return false;
        };
        auto isKw = [](std::string_view tok) {
            static const char* k[] = {
                "and","break","do","else","elseif","end","false","for","function","if","in","local","nil","not","or","repeat","return","then","true","until","while"
            };
            for (const char* s : k) {
                const size_t n = std::strlen(s);
                if (n == tok.size() && std::memcmp(s, tok.data(), n) == 0) return true;
            }
            return false;
        };

        const ImU32 kwCol = IM_COL32(230, 190, 110, 255);
        const ImU32 fnCol = IM_COL32(120, 200, 255, 255);
        const ImU32 cmCol = IM_COL32(140, 150, 160, 255);
        const ImU32 numCol = IM_COL32(160, 220, 160, 255);

        ImFont* font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize();
        auto width = [font, fontSize](std::string_view v) {
            if (v.empty()) return 0.0f;
            return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, v.data(), v.data() + v.size()).x;
        };

        const std::string& src = *text;
        int line = 0;
        int lineStart = 0;
        for (int idx = 0; idx <= static_cast<int>(src.size()); ++idx) {
            const bool eol = (idx == static_cast<int>(src.size())) || (src[idx] == '\n');
            if (!eol) continue;
            if (line >= first && line < last) {
                const int lineEnd = idx;
                const std::string_view sv(src.data() + lineStart, static_cast<size_t>(lineEnd - lineStart));

                size_t commentPos = sv.find("--");
                const std::string_view code = (commentPos == std::string_view::npos) ? sv : sv.substr(0, commentPos);
                if (commentPos != std::string_view::npos) {
                    const std::string_view cm = sv.substr(commentPos);
                    const float x = width(code);
                    dl->AddText(ImVec2(textBase.x + x, textBase.y + line * lineH), cmCol, cm.data(), cm.data() + cm.size());
                }

                size_t j = 0;
                while (j < code.size()) {
                    while (j < code.size() && !isIdent(code[j]) && code[j] != '.' && (std::isdigit(static_cast<unsigned char>(code[j])) == 0)) ++j;
                    if (j >= code.size()) break;

                    if (std::isdigit(static_cast<unsigned char>(code[j])) != 0) {
                        size_t k = j + 1;
                        while (k < code.size() && (std::isdigit(static_cast<unsigned char>(code[k])) != 0 || code[k] == '.')) ++k;
                        const float x = width(code.substr(0, j));
                        const std::string_view num = code.substr(j, k - j);
                        dl->AddText(ImVec2(textBase.x + x, textBase.y + line * lineH), numCol, num.data(), num.data() + num.size());
                        j = k;
                        continue;
                    }

                    if (code[j] == '.') {
                        ++j;
                        continue;
                    }

                    size_t k = j + 1;
                    while (k < code.size() && isIdent(code[k])) ++k;
                    const std::string_view tok = code.substr(j, k - j);
                    ImU32 col = 0;
                    if (isKw(tok)) col = kwCol;
                    else if (isApi(tok)) col = fnCol;
                    if (col != 0) {
                        const float x = width(code.substr(0, j));
                        dl->AddText(ImVec2(textBase.x + x, textBase.y + line * lineH), col, tok.data(), tok.data() + tok.size());
                    }
                    j = k;
                }
            }
            ++line;
            lineStart = idx + 1;
            if (line >= last) break;
        }
    }

    dl->PopClipRect();

    if (ui && !readOnly && ui->assistEnabled && editorActive && ui->completionOpen && !ui->completionMatches.empty()) {
        const auto& docs = LuaEngine::ApiDocs();
        const float popupH = 170.0f * s;
        const float popupW = 420.0f * s;
        const ImVec2 popupPos(winPos.x + gutter + 6.0f * s, winPos.y + winSize.y - popupH - 6.0f * s);
        ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(popupW, popupH), ImGuiCond_Always);
        ImGui::Begin(
            "##lua_completion_popup",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing);

        ImGui::TextDisabled("补全: %s", ui->completionPrefix.c_str());
        ImGui::Separator();
        ImGui::BeginChild("##lua_completion_list", ImVec2(-1, 95.0f * s), true);
        for (int k = 0; k < static_cast<int>(ui->completionMatches.size()) && k < 50; ++k) {
            const int docIdx = ui->completionMatches[k];
            const bool selected = (ui->completionSelected == k);
            const char* name = docs[docIdx].name ? docs[docIdx].name : "";
            if (ImGui::Selectable(name, selected)) ui->completionSelected = k;
            if (ImGui::IsItemHovered() && docs[docIdx].signature) ImGui::SetTooltip("%s", docs[docIdx].signature);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                ui->completionPendingInsert = name;
            }
        }
        ImGui::EndChild();

        const int sel = std::clamp(ui->completionSelected, 0, static_cast<int>(ui->completionMatches.size()) - 1);
        const int docIdx = ui->completionMatches[sel];
        if (docs[docIdx].signature) ImGui::TextDisabled("%s", docs[docIdx].signature);
        if (docs[docIdx].brief) ImGui::TextWrapped("%s", docs[docIdx].brief);
        if (ImGui::Button("插入所选")) {
            const char* name = docs[docIdx].name ? docs[docIdx].name : "";
            ui->completionPendingInsert = name;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Tab: 完成  Ctrl+Space: 打开  Esc: 关闭  双击: 插入");

        ImGui::End();
    }

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

struct InputTextChainCtx {
    std::string* str;
    ImGuiInputTextCallback callback;
    void* userData;
};

static int ImGuiStringResizeChainCallback(ImGuiInputTextCallbackData* data) {
    auto* ctx = static_cast<InputTextChainCtx*>(data->UserData);
    if (!ctx || !ctx->str) return 0;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        ctx->str->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = ctx->str->data();
        return 0;
    }
    if (!ctx->callback) return 0;
    void* old = data->UserData;
    data->UserData = ctx->userData;
    const int r = ctx->callback(data);
    data->UserData = old;
    return r;
}

static bool InputTextMultilineStringWithCallback(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags extraFlags, ImGuiInputTextCallback callback, void* userData) {
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize | extraFlags;
    if (str->capacity() < 4096) str->reserve(4096);
    str->resize(str->capacity());
    InputTextChainCtx ctx{ str, callback, userData };
    const bool changed = ImGui::InputTextMultiline(label, str->data(), str->size() + 1, size, flags, ImGuiStringResizeChainCallback, &ctx);
    str->resize(std::strlen(str->c_str()));
    return changed;
}

static bool ContainsCaseInsensitive(const char* s, const std::string& needle) {
    if (!s) return false;
    if (needle.empty()) return true;
    const size_t n = needle.size();
    for (const char* p = s; *p; ++p) {
        size_t i = 0;
        while (i < n && p[i] && std::tolower(static_cast<unsigned char>(p[i])) == std::tolower(static_cast<unsigned char>(needle[i]))) ++i;
        if (i == n) return true;
    }
    return false;
}

static bool IsIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

static void BuildCompletionMatches(LuaScriptUiState* ui, const char* buf, int cursorPos) {
    if (!ui || !buf) return;
    const int len = static_cast<int>(std::strlen(buf));
    const int cur = std::clamp(cursorPos, 0, len);
    int start = cur;
    while (start > 0 && IsIdentChar(buf[start - 1])) --start;
    ui->completionCursorPos = cur;
    ui->completionWordStart = start;
    ui->completionPrefix.assign(buf + start, buf + cur);

    ui->completionMatches.clear();
    if (!ui->assistEnabled) return;
    if (ui->completionPrefix.empty()) return;

    const auto& docs = LuaEngine::ApiDocs();
    ui->completionMatches.reserve(32);
    for (int i = 0; i < static_cast<int>(docs.size()); ++i) {
        const char* name = docs[i].name;
        if (!name) continue;
        if (std::strncmp(name, ui->completionPrefix.c_str(), ui->completionPrefix.size()) == 0) ui->completionMatches.push_back(i);
    }
    if (ui->completionSelected >= static_cast<int>(ui->completionMatches.size())) ui->completionSelected = 0;
}

static std::string CommonPrefix(const LuaScriptUiState* ui) {
    if (!ui || ui->completionMatches.empty()) return {};
    const auto& docs = LuaEngine::ApiDocs();
    std::string p = docs[ui->completionMatches[0]].name ? docs[ui->completionMatches[0]].name : "";
    for (size_t i = 1; i < ui->completionMatches.size(); ++i) {
        const char* s = docs[ui->completionMatches[i]].name ? docs[ui->completionMatches[i]].name : "";
        size_t j = 0;
        while (j < p.size() && s[j] && p[j] == s[j]) ++j;
        p.resize(j);
        if (p.empty()) break;
    }
    return p;
}

static int LuaEditorInputCallback(ImGuiInputTextCallbackData* data) {
    auto* ui = static_cast<LuaScriptUiState*>(data->UserData);
    if (!ui) return 0;

    if (data->Buf) BuildCompletionMatches(ui, data->Buf, data->CursorPos);

    if (!ui->completionPendingInsert.empty()) {
        const std::string insert = ui->completionPendingInsert;
        ui->completionPendingInsert.clear();
        const int start = std::clamp(ui->completionWordStart, 0, data->BufTextLen);
        const int cur = std::clamp(ui->completionCursorPos, 0, data->BufTextLen);
        if (cur >= start) {
            data->DeleteChars(start, cur - start);
            data->InsertChars(start, insert.c_str());
            data->CursorPos = start + static_cast<int>(insert.size());
        }
        ui->completionOpen = false;
        return 0;
    }

    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        if (data->Buf) BuildCompletionMatches(ui, data->Buf, data->CursorPos);
        if (ui->completionMatches.empty()) return 0;
        const auto& docs = LuaEngine::ApiDocs();
        const int start = std::clamp(ui->completionWordStart, 0, data->BufTextLen);
        const int cur = std::clamp(ui->completionCursorPos, 0, data->BufTextLen);

        if (ui->completionMatches.size() == 1) {
            const char* name = docs[ui->completionMatches[0]].name;
            if (!name) return 0;
            data->DeleteChars(start, cur - start);
            data->InsertChars(start, name);
            data->CursorPos = start + static_cast<int>(std::strlen(name));
            ui->completionOpen = false;
            return 0;
        }

        const std::string common = CommonPrefix(ui);
        if (!common.empty() && common.size() > ui->completionPrefix.size()) {
            data->DeleteChars(start, cur - start);
            data->InsertChars(start, common.c_str());
            data->CursorPos = start + static_cast<int>(common.size());
            ui->completionOpen = true;
            return 0;
        }

        ui->completionOpen = true;
        return 0;
    }

    return 0;
}

static void DrawLuaDocsPanel(LuaScriptUiState* ui, float height, bool disabled) {
    if (!ui) return;
    const auto& docs = LuaEngine::ApiDocs();
    ImGui::BeginChild("##lua_docs_panel", ImVec2(-1, height), true);

    ImGui::TextUnformatted("Lua API");
    ImGui::SameLine();
    ImGui::Checkbox("提示/补全", &ui->assistEnabled);
    if (disabled) ImGui::BeginDisabled();
    InputTextString("搜索##lua_docs", &ui->docsFilter);
    if (disabled) ImGui::EndDisabled();

    ImGui::Separator();

    ImGui::BeginChild("##lua_docs_list", ImVec2(-1, height * 0.55f), true);
    for (int i = 0; i < static_cast<int>(docs.size()); ++i) {
        const auto& d = docs[i];
        const bool pass = ui->docsFilter.empty() ||
            ContainsCaseInsensitive(d.name, ui->docsFilter) ||
            ContainsCaseInsensitive(d.signature, ui->docsFilter) ||
            ContainsCaseInsensitive(d.group, ui->docsFilter);
        if (!pass) continue;
        const bool selected = (ui->docsSelected == i);
        std::string label = (d.group ? std::string(d.group) : std::string()) + "  " + (d.name ? d.name : "");
        if (ImGui::Selectable(label.c_str(), selected)) ui->docsSelected = i;
    }
    ImGui::EndChild();

    ImGui::Separator();

    ImGui::BeginChild("##lua_docs_detail", ImVec2(-1, 0), true);
    const int sel = ui->docsSelected;
    if (sel >= 0 && sel < static_cast<int>(docs.size())) {
        const auto& d = docs[sel];
        if (d.name) ImGui::Text("%s", d.name);
        if (d.signature) ImGui::TextDisabled("%s", d.signature);
        if (d.brief) ImGui::TextWrapped("%s", d.brief);
        if (ImGui::Button("复制签名") && d.signature) ImGui::SetClipboardText(d.signature);
    } else {
        ImGui::TextDisabled("选择一个函数查看说明");
    }
    ImGui::EndChild();
    ImGui::EndChild();
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
    // 视觉样式微调：更紧凑、更现代
    style.WindowPadding = ImVec2(10.0f * s, 10.0f * s);
    style.FramePadding = ImVec2(5.0f * s, 3.0f * s);
    style.ItemSpacing = ImVec2(6.0f * s, 5.0f * s);
    style.ItemInnerSpacing = ImVec2(5.0f * s, 3.0f * s);
    style.WindowRounding = 6.0f * s;
    style.FrameRounding = 4.0f * s;
    style.GrabRounding = 4.0f * s;
    style.ScrollbarRounding = 4.0f * s;
    style.ChildRounding = 4.0f * s;
    style.PopupRounding = 4.0f * s;

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

    // 头部区域 - 紧凑设计
    const float headerH = 40.0f * s;
    ImGui::BeginChild("##header", ImVec2(0, headerH), true, ImGuiWindowFlags_NoScrollbar);
    const float logoSize = 24.0f * s;
    ImGui::SetCursorPosY((headerH - logoSize) * 0.5f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f * s);
    DrawAppLogo(ImVec2(logoSize, logoSize));
    ImGui::SameLine();
    
    ImGui::BeginGroup();
    ImGui::SetCursorPosY((headerH - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.20f, 0.56f, 0.86f, 1.0f));
    ImGui::Text("AutoClicker-Pro");
    ImGui::PopStyleColor();
    ImGui::EndGroup();

    // 紧急停止提示
    ImGui::SameLine();
    ImGui::SetCursorPosY((headerH - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::TextDisabled(" |  Ctrl+F12: 紧急停止");
    
    // 模式切换放在头部右侧
    ImGui::SameLine();
    const float modeWidth = 260.0f * s;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - modeWidth - ImGui::GetStyle().WindowPadding.x);
    ImGui::SetCursorPosY((headerH - ImGui::GetFrameHeight()) * 0.5f);
    if (ImGui::RadioButton("简单模式", &mode_, 0)) {
        // 切换模式时重置一些状态
    }
    ImGui::SameLine(0, 20.0f * s);
    ImGui::RadioButton("高级模式 (Lua)", &mode_, 1);
    ImGui::EndChild();

    // 移除 Header 与 Content 之间的大间距，只留一点点
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f * s);

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

    ImVec4 c = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
    ImVec4 bgColor = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
    
    if (statusLevel_ == 1) {
        c = ImVec4(0.13f, 0.59f, 0.29f, 1.0f);  // 绿色
        bgColor = ImVec4(0.85f, 0.98f, 0.90f, 1.0f);
    }
    if (statusLevel_ == 2) {
        c = ImVec4(0.85f, 0.55f, 0.10f, 1.0f);  // 橙色
        bgColor = ImVec4(1.00f, 0.95f, 0.85f, 1.0f);
    }
    if (statusLevel_ == 3) {
        c = ImVec4(0.85f, 0.15f, 0.15f, 1.0f);  // 红色
        bgColor = ImVec4(1.00f, 0.90f, 0.90f, 1.0f);
    }

    const float s = UiScale();
    // 移除不必要的 Spacing，保持紧凑
    
    // 状态栏 - 再压缩高度
    ImGui::BeginChild("##status_bar", ImVec2(0, 28.0f * s), true, ImGuiWindowFlags_NoScrollbar);
    
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 winPos = ImGui::GetWindowPos();
    const ImVec2 winSize = ImGui::GetWindowSize();
    dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), ImGui::GetColorU32(bgColor));
    
    // 文字垂直居中
    ImGui::SetCursorPosY((winSize.y - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f * s);
    
    // 文本自动换行
    const float availWidth = ImGui::GetContentRegionAvail().x;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + availWidth);
    ImGui::TextColored(c, "%s", statusText_.c_str());
    ImGui::PopTextWrapPos();
    
    ImGui::EndChild();
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

static std::string FormatEvent(const trc::RawEvent& e) {
    char buf[128];
    const auto type = static_cast<trc::EventType>(e.type);
    switch (type) {
    case trc::EventType::MouseMove:
        snprintf(buf, sizeof(buf), "Move (%d, %d)", e.x, e.y);
        break;
    case trc::EventType::MouseDown:
        snprintf(buf, sizeof(buf), "Down %s (%d, %d)", 
            e.data == 0 ? "L" : (e.data == 1 ? "R" : "M"), e.x, e.y);
        break;
    case trc::EventType::MouseUp:
        snprintf(buf, sizeof(buf), "Up %s (%d, %d)", 
            e.data == 0 ? "L" : (e.data == 1 ? "R" : "M"), e.x, e.y);
        break;
    case trc::EventType::Wheel:
        snprintf(buf, sizeof(buf), "Wheel %d", e.data);
        break;
    case trc::EventType::KeyDown:
        snprintf(buf, sizeof(buf), "Key Down 0x%02X", e.data);
        break;
    case trc::EventType::KeyUp:
        snprintf(buf, sizeof(buf), "Key Up 0x%02X", e.data);
        break;
    default:
        snprintf(buf, sizeof(buf), "Unknown (%d)", e.type);
        break;
    }
    
    // Append delay info if significant
    if (e.timeDelta > 1000) {
        std::string s(buf);
        char t[32];
        snprintf(t, sizeof(t), " (+%lld ms)", e.timeDelta / 1000);
        return s + t;
    }
    return std::string(buf);
}

void App::DrawSimpleMode() {
    const float s = UiScale();
    const float availWidth = ImGui::GetContentRegionAvail().x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    
    // --- 1. 配置区域 (紧凑) ---
    // 第一行：文件操作
    ImGui::TextUnformatted("脚本文件");
    ImGui::SameLine();
    
    const float browseBtnW = 60.0f * s;
    const float saveBtnW = 60.0f * s;
    const float fileLabelW = ImGui::CalcTextSize("脚本文件").x;
    
    ImGui::SetNextItemWidth(availWidth - fileLabelW - browseBtnW - saveBtnW - spacing * 4 - ImGui::GetStyle().WindowPadding.x);
    InputTextString("##path", &trcPath_);
    
    ImGui::SameLine();
    if (ImGui::Button("浏览...", ImVec2(browseBtnW, 0))) {
        wchar_t buf[MAX_PATH]{};
        std::wstring w = Utf8ToWide(trcPath_);
        wcsncpy_s(buf, w.c_str(), _TRUNCATE);
        if (OpenFileDialog(nullptr, buf, MAX_PATH, L"Trace File (*.trc)\0*.trc\0\0")) trcPath_ = WideToUtf8(std::wstring(buf));
    }
    
    ImGui::SameLine();
    if (ImGui::Button("另存为", ImVec2(saveBtnW, 0))) {
        wchar_t buf[MAX_PATH]{};
        std::wstring w = Utf8ToWide(trcPath_);
        wcsncpy_s(buf, w.c_str(), _TRUNCATE);
        if (SaveFileDialog(nullptr, buf, MAX_PATH, L"Trace File (*.trc)\0*.trc\0\0")) {
            trcPath_ = WideToUtf8(std::wstring(buf));
            recorder_.SaveToFile(Utf8ToWide(trcPath_));
            SetStatusOk("已保存录制文件");
        }
    }

    // 第二行：参数设置
    ImGui::AlignTextToFramePadding();
    ImGui::Text("回放设置");
    ImGui::SameLine();
    
    ImGui::SetNextItemWidth(120.0f * s);
    if (ImGui::SliderFloat("##speed", &speedFactor_, 0.1f, 10.0f, "倍速: %.1fx")) {
        replayer_.SetSpeed(speedFactor_);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("回放速度倍率 (0.1 - 10.0)");
    
    ImGui::SameLine();
    ImGui::Checkbox("屏蔽输入", &blockInput_);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("回放时屏蔽物理键鼠输入");

    ImGui::Separator();

    // --- 2. 列表区域 (自适应) ---
    // 预留底部按钮区域高度 (约 40px)
    const float bottomHeight = 40.0f * s;
    const float listHeight = std::max(100.0f * s, ImGui::GetContentRegionAvail().y - bottomHeight - 4.0f * s);
    
    ImGui::TextDisabled("事件列表 (%zu 个事件)", recorder_.Events().size());
    
    if (ImGui::BeginChild("##event_list", ImVec2(0, listHeight), true)) {
        // 使用 Clipper 优化大列表性能
        ImGuiListClipper clipper;
        clipper.Begin((int)recorder_.Events().size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                const auto& evt = recorder_.Events()[i];
                ImGui::Text("%04d | %s", i + 1, FormatEvent(evt).c_str());
            }
        }
        
        // 自动滚动到底部
        if (recorder_.IsRecording() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
            
        ImGui::EndChild();
    }

    // --- 3. 底部操作栏 ---
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f * s);
    const float btnH = 32.0f * s;
    
    if (!recorder_.IsRecording() && !replayer_.IsRunning()) {
        // 空闲状态：显示 [加载] [录制] [回放]
        const float btnW = (availWidth - spacing * 2) / 3.0f;
        
        if (ImGui::Button("加载文件", ImVec2(btnW, btnH))) {
            if (recorder_.LoadFromFile(Utf8ToWide(trcPath_))) SetStatusOk("已加载录制文件");
            else SetStatusError("加载失败");
        }
        
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // 红色录制
        if (ImGui::Button("开始录制 (F9)", ImVec2(btnW, btnH))) {
            StartRecording();
        }
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f)); // 绿色回放
        if (ImGui::Button("开始回放 (F10)", ImVec2(btnW, btnH))) {
            StartReplay();
        }
        ImGui::PopStyleColor();
        
    } else {
        // 工作状态：显示 [停止]
        const float btnW = availWidth * 0.6f;
        ImGui::SetCursorPosX((availWidth - btnW) * 0.5f); // 居中
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f)); // 橙色停止
        std::string stopLabel = recorder_.IsRecording() ? "停止录制 (F9)" : "停止回放 (F10)";
        if (ImGui::Button(stopLabel.c_str(), ImVec2(btnW, btnH))) {
            if (recorder_.IsRecording()) StopRecording();
            else StopReplay();
        }
        ImGui::PopStyleColor();
    }
}

void App::DrawAdvancedMode() {
    const float s = UiScale();
    const bool scriptRunning = lua_.IsRunning();
    const float availWidth = ImGui::GetContentRegionAvail().x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    
    // --- 1. 文件栏 ---
    {
        ImGui::TextUnformatted("Lua 文件");
        ImGui::SameLine();
        const float browseWidth = 52.0f * s;
        const float saveWidth = 52.0f * s;
        const float fileLabelW = ImGui::CalcTextSize("Lua 文件").x;
        
        // 路径输入框占据主要宽度
        ImGui::SetNextItemWidth(availWidth - fileLabelW - browseWidth - saveWidth * 2 - spacing * 4 - ImGui::GetStyle().WindowPadding.x);
        InputTextString("##luapath", &luaPath_);
        
        ImGui::SameLine();
        if (ImGui::Button("浏览...", ImVec2(browseWidth, 0))) {
            wchar_t buf[MAX_PATH]{};
            std::wstring w = Utf8ToWide(luaPath_);
            wcsncpy_s(buf, w.c_str(), _TRUNCATE);
            if (OpenFileDialog(nullptr, buf, MAX_PATH, L"Lua Script (*.lua)\0*.lua\0\0")) luaPath_ = WideToUtf8(std::wstring(buf));
        }
        
        ImGui::SameLine();
        if (ImGui::Button("加载", ImVec2(saveWidth, 0))) {
            luaEditor_ = ReadTextFile(Utf8ToWide(luaPath_));
            luaLastError_.clear();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("从文件加载内容到编辑器");

        ImGui::SameLine();
        if (ImGui::Button("保存", ImVec2(saveWidth, 0))) {
            WriteTextFile(Utf8ToWide(luaPath_), luaEditor_);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("保存编辑器内容到文件");
    }

    // --- 2. 工具栏 (Toolbar) ---
    ImGui::Spacing();
    {
        const float btnH = 28.0f * s; // 紧凑高度
        
        // 运行/停止
        if (!scriptRunning) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.59f, 0.29f, 1.0f)); // 绿色
            if (ImGui::Button("▶ 运行", ImVec2(80.0f * s, btnH))) {
                luaLastError_.clear();
                if (!lua_.StartAsync(luaEditor_)) {
                    SetStatusError("脚本启动失败");
                } else {
                    // ... 启动逻辑保持不变 ...
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
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.15f, 0.15f, 1.0f)); // 红色
            if (ImGui::Button("■ 停止", ImVec2(80.0f * s, btnH))) {
                lua_.StopAsync();
                SetStatusInfo("已停止脚本");
            }
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();
        ImGui::Separator(); // 使用标准 Separator
        ImGui::SameLine();

        // 选项开关
        ImGui::Checkbox("运行时最小化", &minimizeOnScriptRun_);
        ImGui::SameLine();
        ImGui::Checkbox("文档", &luaUi_.docsOpen);
        ImGui::SameLine();
        ImGui::Checkbox("补全", &luaUi_.assistEnabled);

        // 更多工具 (下拉菜单)
        ImGui::SameLine();
        const float toolBtnWidth = 100.0f * s;
        const float rightAlignPos = availWidth - toolBtnWidth - ImGui::GetStyle().WindowPadding.x;
        if (ImGui::GetCursorPosX() < rightAlignPos) ImGui::SetCursorPosX(rightAlignPos);
        
        if (ImGui::Button("更多工具 ▼", ImVec2(toolBtnWidth, btnH))) {
            ImGui::OpenPopup("more_tools_popup");
        }

        if (ImGui::BeginPopup("more_tools_popup")) {
            ImGui::TextDisabled("转换工具 (TRC -> Lua)");
            ImGui::Separator();
            
            static float tol = 3.0f;
            ImGui::SetNextItemWidth(120.0f * s);
            ImGui::SliderFloat("容差##tol", &tol, 0.5f, 20.0f, "%.1f px");
            ImGui::Checkbox("高保真导出", &exportFull_);
            
            if (ImGui::Button("执行转换", ImVec2(-1, 0))) {
                bool ok = false;
                if (exportFull_) ok = Converter::TrcToLuaFull(Utf8ToWide(trcPath_), Utf8ToWide(luaPath_));
                else ok = Converter::TrcToLua(Utf8ToWide(trcPath_), Utf8ToWide(luaPath_), tol);
                if (ok) {
                    luaEditor_ = ReadTextFile(Utf8ToWide(luaPath_));
                    luaLastError_.clear();
                    SetStatusOk("导出成功");
                    ImGui::CloseCurrentPopup();
                } else {
                    SetStatusError("导出失败");
                }
            }
            ImGui::EndPopup();
        }
    }

    // --- 3. 错误信息显示 (如有) ---
    if (!luaLastError_.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.2f, 1));
        ImGui::TextWrapped("错误: %s", luaLastError_.c_str());
        ImGui::PopStyleColor();
    }

    // --- 4. 编辑器区域 (填满剩余空间) ---
    ImGui::Spacing();
    const int curLine = scriptRunning ? lua_.CurrentLine() : 0;
    // 计算剩余高度：总可用 - 当前光标位置 - 少量底部Padding
    const float remainHeight = std::max(100.0f * s, ImGui::GetContentRegionAvail().y - 4.0f * s); 
    
    if (luaUi_.docsOpen) {
        if (ImGui::BeginTable("##lua_layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0, remainHeight))) {
            ImGui::TableSetupColumn("编辑器", ImGuiTableColumnFlags_WidthStretch, 0.75f);
            ImGui::TableSetupColumn("文档", ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableNextColumn();
            
            // 编辑器列
            if (scriptRunning) {
                if (curLine > 0) ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1), "正在执行行: %d", curLine);
                else ImGui::TextDisabled("正在执行...");
            } else {
                ImGui::TextDisabled("编辑器");
            }
            DrawLuaEditorWithLineNumbers(&luaUi_, &luaEditor_, -1.0f, scriptRunning, curLine, &luaLastHighlightLine_); // -1.0f 让 Child 自动填满 Cell
            
            ImGui::TableNextColumn();
            // 文档列
            DrawLuaDocsPanel(&luaUi_, -1.0f, false);
            
            ImGui::EndTable();
        }
    } else {
        // 无文档，全屏编辑器
        if (scriptRunning) {
            if (curLine > 0) ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1), "正在执行行: %d", curLine);
            else ImGui::TextDisabled("正在执行...");
        }
        // 使用 BeginChild 包装一下以确保高度控制
        ImGui::BeginChild("##editor_container", ImVec2(0, remainHeight), false);
        DrawLuaEditorWithLineNumbers(&luaUi_, &luaEditor_, remainHeight - ImGui::GetTextLineHeightWithSpacing(), scriptRunning, curLine, &luaLastHighlightLine_);
        ImGui::EndChild();
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
