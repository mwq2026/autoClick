#include "app/App.h"

#include <algorithm>
#include <cmath>
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
#include <imgui_internal.h>

#include "core/Converter.h"
#include "core/HighResClock.h"

static bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags extraFlags);
static bool InputTextMultilineStringWithCallback(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags extraFlags, ImGuiInputTextCallback callback, void* userData);
static int LuaEditorInputCallback(ImGuiInputTextCallbackData* data);

// ─── Constants ──────────────────────────────────────────────────────────────

static constexpr int kParticleCount = 30;
static constexpr float kPi = 3.14159265f;

// ─── Color palette ──────────────────────────────────────────────────────────
// Gradient: deep blue (#1a1a3e) → purple (#4a2080) → magenta (#7b2d8e)
// Accent:   cyan (#00d4ff), pink (#ff6ec7), gold (#ffd700)

static ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const int ra = (a >> 0) & 0xFF, ga = (a >> 8) & 0xFF, ba = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
    const int rb = (b >> 0) & 0xFF, gb = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF, ab = (b >> 24) & 0xFF;
    return IM_COL32(
        ra + (int)((rb - ra) * t),
        ga + (int)((gb - ga) * t),
        ba + (int)((bb - ba) * t),
        aa + (int)((ab - aa) * t));
}

static ImU32 ColorWithAlpha(ImU32 col, float alpha) {
    return (col & 0x00FFFFFF) | ((ImU32)(alpha * 255.0f) << 24);
}

// ─── Utility ────────────────────────────────────────────────────────────────

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
    if (!OpenProcessToken(h, TOKEN_QUERY, &token)) { CloseHandle(h); return false; }
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

// ─── Gradient & glass drawing helpers ───────────────────────────────────────

// Draw a vertical gradient rect
static void DrawGradientRect(ImDrawList* dl, ImVec2 tl, ImVec2 br, ImU32 colTop, ImU32 colBot) {
    dl->AddRectFilledMultiColor(tl, br, colTop, colTop, colBot, colBot);
}

// Glass card: semi-transparent with subtle border glow
static void BeginGlassCard(const char* id, const char* title, const ImVec2& size, float rounding = 0.0f) {
    const float s = UiScale();
    const float r = rounding > 0.0f ? rounding : 10.0f * s;
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, r);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.15f, 0.30f, 0.60f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.55f, 0.45f, 0.85f, 0.40f));
    ImGui::BeginChild(id, size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Subtle inner glow at top
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    dl->AddRectFilledMultiColor(
        wp, ImVec2(wp.x + ws.x, wp.y + 3.0f * s),
        IM_COL32(140, 120, 255, 40), IM_COL32(200, 100, 255, 40),
        IM_COL32(200, 100, 255, 0), IM_COL32(140, 120, 255, 0));

    if (title && title[0]) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.75f, 0.95f, 1.0f));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
}

static void BeginGlassScrollCard(const char* id, const char* title, const ImVec2& size) {
    const float s = UiScale();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f * s);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.15f, 0.30f, 0.60f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.55f, 0.45f, 0.85f, 0.40f));
    ImGui::BeginChild(id, size, true);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    dl->AddRectFilledMultiColor(
        wp, ImVec2(wp.x + ws.x, wp.y + 3.0f * s),
        IM_COL32(140, 120, 255, 40), IM_COL32(200, 100, 255, 40),
        IM_COL32(200, 100, 255, 0), IM_COL32(140, 120, 255, 0));

    if (title && title[0]) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.75f, 0.95f, 1.0f));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
}

static void EndGlassCard() {
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// Gradient button with glow
static bool GlowButton(const char* label, const ImVec2& size, ImU32 colLeft, ImU32 colRight, float rounding = 0.0f) {
    const float s = UiScale();
    const float r = rounding > 0.0f ? rounding : 6.0f * s;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 br = ImVec2(pos.x + size.x, pos.y + size.y);

    ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Glow behind button on hover
    if (hovered) {
        const ImU32 glow = ColorWithAlpha(colLeft, 0.25f);
        dl->AddRectFilled(
            ImVec2(pos.x - 3.0f * s, pos.y - 3.0f * s),
            ImVec2(br.x + 3.0f * s, br.y + 3.0f * s),
            glow, r + 3.0f * s);
    }

    // Button body gradient
    const float darken = active ? 0.7f : (hovered ? 0.85f : 1.0f);
    const ImU32 cL = LerpColor(IM_COL32(0, 0, 0, 255), colLeft, darken);
    const ImU32 cR = LerpColor(IM_COL32(0, 0, 0, 255), colRight, darken);
    dl->AddRectFilledMultiColor(pos, br, cL, cR, cR, cL);
    // Rounded corners overlay (draw rounded rect border to mask corners)
    dl->AddRect(pos, br, IM_COL32(255, 255, 255, hovered ? 80 : 40), r, 0, 1.5f * s);

    // Label centered
    const ImVec2 textSz = ImGui::CalcTextSize(label);
    dl->AddText(
        ImVec2(pos.x + (size.x - textSz.x) * 0.5f, pos.y + (size.y - textSz.y) * 0.5f),
        IM_COL32(255, 255, 255, 240), label);

    return clicked;
}

// ─── Animated cursor icon ───────────────────────────────────────────────────

void App::DrawAnimatedCursor(ImVec2 center, float radius, float time) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = UiScale();
    const float r = radius;

    // === Design: glowing circle with a bold "click" pointer inside ===

    // 1) Outer breathing glow
    const float breathe = 0.6f + 0.4f * sinf(time * 2.2f);
    dl->AddCircleFilled(center, r * 1.10f, IM_COL32(100, 80, 220, (int)(30 * breathe)), 48);

    // 2) Main circle — gradient from deep indigo to vivid purple
    dl->AddCircleFilled(center, r * 0.92f, IM_COL32(45, 30, 110, 255), 48);
    dl->AddCircleFilled(center, r * 0.78f, IM_COL32(70, 50, 160, 255), 48);
    // Bright highlight at top-left for 3D feel
    dl->AddCircleFilled(ImVec2(center.x - r * 0.18f, center.y - r * 0.18f),
        r * 0.38f, IM_COL32(120, 100, 220, 60), 32);

    // 3) Click ripple effect — rings expanding outward
    const float clickPeriod = 1.5f;
    const float clickPhase = fmodf(time, clickPeriod) / clickPeriod;
    for (int i = 0; i < 2; ++i) {
        float phase = clickPhase - i * 0.18f;
        if (phase < 0.0f) phase += 1.0f;
        if (phase < 0.6f) {
            const float t = phase / 0.6f;
            const float rr = r * (0.35f + t * 0.75f);
            const float a = (1.0f - t * t) * 0.5f;
            dl->AddCircle(center, rr, IM_COL32(160, 200, 255, (int)(a * 255)), 48, 1.5f * s);
        }
    }

    // 4) Bold pointer arrow — simple triangle, very clear at any size
    //    Points upward-left like a classic cursor
    const float bobX = sinf(time * 1.8f) * r * 0.02f;
    const float bobY = cosf(time * 2.3f) * r * 0.025f;
    const float ax = center.x + bobX;
    const float ay = center.y + bobY;
    const float sz = r * 0.42f;

    // Simple 3-point arrow (large triangle) — always crisp
    const ImVec2 arrowTip(ax - sz * 0.50f, ay - sz * 0.55f);
    const ImVec2 arrowBL (ax - sz * 0.50f, ay + sz * 0.60f);
    const ImVec2 arrowBR (ax + sz * 0.55f, ay + sz * 0.10f);

    // Shadow
    const ImVec2 sTip(arrowTip.x + 1.5f * s, arrowTip.y + 1.5f * s);
    const ImVec2 sBL (arrowBL.x  + 1.5f * s, arrowBL.y  + 1.5f * s);
    const ImVec2 sBR (arrowBR.x  + 1.5f * s, arrowBR.y  + 1.5f * s);
    dl->AddTriangleFilled(sTip, sBL, sBR, IM_COL32(20, 10, 50, 120));

    // Fill — bright gradient cyan-white, pulsing
    const float cp = 0.5f + 0.5f * sinf(time * 1.5f);
    const ImU32 arrowCol = IM_COL32(
        (int)(220 + 35 * cp), (int)(240 + 15 * cp), 255, 255);
    dl->AddTriangleFilled(arrowTip, arrowBL, arrowBR, arrowCol);

    // Crisp dark outline
    dl->AddTriangle(arrowTip, arrowBL, arrowBR, IM_COL32(25, 15, 70, 255), 2.0f * s);

    // 5) Click flash at arrow tip
    if (clickPhase < 0.08f) {
        const float flash = 1.0f - clickPhase / 0.08f;
        dl->AddCircleFilled(arrowTip, 3.0f * s,
            IM_COL32(255, 255, 255, (int)(flash * 220)), 16);
    }

    // 6) Thin bright ring border
    dl->AddCircle(center, r * 0.92f, IM_COL32(140, 120, 255, (int)(80 + 40 * breathe)), 48, 1.2f * s);
}

// ─── Animated taskbar icon ──────────────────────────────────────────────────

void App::UpdateTaskbarIcon() {
    // Update every ~100ms for smooth animation
    if (animTime_ - lastIconUpdateTime_ < 0.10f) return;
    lastIconUpdateTime_ = animTime_;

    const int sz = 32;
    std::vector<DWORD> px(sz * sz, 0);

    auto blend = [&](int x, int y, BYTE r, BYTE g, BYTE b, float a) {
        if (x < 0 || x >= sz || y < 0 || y >= sz || a <= 0.0f) return;
        a = std::min(a, 1.0f);
        DWORD& dst = px[y * sz + x];
        BYTE da = (BYTE)(dst >> 24);
        if (da == 0) {
            dst = ((DWORD)(BYTE)(a * 255) << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
            return;
        }
        float fa = a, fb = da / 255.0f, oa = fa + fb * (1.0f - fa);
        if (oa < 0.001f) return;
        dst = ((DWORD)(BYTE)(oa * 255) << 24)
            | ((DWORD)(BYTE)((r * fa + ((dst >> 16) & 0xFF) * fb * (1.0f - fa)) / oa) << 16)
            | ((DWORD)(BYTE)((g * fa + ((dst >> 8) & 0xFF) * fb * (1.0f - fa)) / oa) << 8)
            | (DWORD)(BYTE)((b * fa + (dst & 0xFF) * fb * (1.0f - fa)) / oa);
    };

    auto fillCircle = [&](float cx, float cy, float rad, BYTE r, BYTE g, BYTE b, float a) {
        int x0 = std::max(0, (int)(cx - rad - 1));
        int x1 = std::min(sz - 1, (int)(cx + rad + 1));
        int y0 = std::max(0, (int)(cy - rad - 1));
        int y1 = std::min(sz - 1, (int)(cy + rad + 1));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                float d = sqrtf(dx * dx + dy * dy);
                if (d <= rad) blend(x, y, r, g, b, a);
                else if (d <= rad + 1.0f) blend(x, y, r, g, b, a * (rad + 1.0f - d));
            }
    };

    auto fillTriangle = [&](float x0, float y0, float x1, float y1, float x2, float y2,
                            BYTE r, BYTE g, BYTE b, float a) {
        int minX = std::max(0, (int)std::min({x0, x1, x2}));
        int maxX = std::min(sz - 1, (int)std::max({x0, x1, x2}) + 1);
        int minY = std::max(0, (int)std::min({y0, y1, y2}));
        int maxY = std::min(sz - 1, (int)std::max({y0, y1, y2}) + 1);
        for (int y = minY; y <= maxY; ++y)
            for (int x = minX; x <= maxX; ++x) {
                float px_ = x + 0.5f, py_ = y + 0.5f;
                float d0 = (x1 - x0) * (py_ - y0) - (y1 - y0) * (px_ - x0);
                float d1 = (x2 - x1) * (py_ - y1) - (y2 - y1) * (px_ - x1);
                float d2 = (x0 - x2) * (py_ - y2) - (y0 - y2) * (px_ - x2);
                if ((d0 >= 0 && d1 >= 0 && d2 >= 0) || (d0 <= 0 && d1 <= 0 && d2 <= 0))
                    blend(x, y, r, g, b, a);
            }
    };

    const float cx = 16.0f, cy = 16.0f;
    const float time = animTime_;

    // Background circle
    fillCircle(cx, cy, 14.5f, 45, 30, 110, 1.0f);
    fillCircle(cx, cy, 12.0f, 70, 50, 160, 1.0f);

    // Ripple
    const float clickPhase = fmodf(time, 1.5f) / 1.5f;
    if (clickPhase < 0.6f) {
        float t = clickPhase / 0.6f;
        float rr = 5.0f + t * 10.0f;
        float a = (1.0f - t * t) * 0.35f;
        // Draw ring as filled circle minus inner
        for (int y = 0; y < sz; ++y)
            for (int x = 0; x < sz; ++x) {
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                float d = sqrtf(dx * dx + dy * dy);
                if (d >= rr - 1.0f && d <= rr + 1.0f) {
                    float aa = a * (1.0f - fabsf(d - rr));
                    if (aa > 0) blend(x, y, 160, 200, 255, aa);
                }
            }
    }

    // Arrow triangle
    const float cp = 0.5f + 0.5f * sinf(time * 1.5f);
    const BYTE ar = (BYTE)(220 + 35 * cp), ag = (BYTE)(240 + 15 * cp), ab = 255;
    float bobX_ = sinf(time * 1.8f) * 0.3f;
    float bobY_ = cosf(time * 2.3f) * 0.4f;
    float ax_ = cx + bobX_, ay_ = cy + bobY_;
    float asz = 6.5f;
    fillTriangle(ax_ - asz * 0.50f, ay_ - asz * 0.55f,
                 ax_ - asz * 0.50f, ay_ + asz * 0.60f,
                 ax_ + asz * 0.55f, ay_ + asz * 0.10f,
                 ar, ag, ab, 1.0f);

    // Border ring
    float breathe_ = 0.6f + 0.4f * sinf(time * 2.2f);
    for (int y = 0; y < sz; ++y)
        for (int x = 0; x < sz; ++x) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float d = sqrtf(dx * dx + dy * dy);
            if (d >= 13.5f && d <= 15.0f) {
                float aa = (0.25f + 0.12f * breathe_) * (1.0f - fabsf(d - 14.2f) / 0.8f);
                if (aa > 0) blend(x, y, 140, 120, 255, aa);
            }
        }

    HICON newIcon = CreateIcon(GetModuleHandle(nullptr), sz, sz, 1, 32, nullptr, (BYTE*)px.data());
    if (newIcon) {
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)newIcon);
        SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)newIcon);
        if (taskbarIcon_) DestroyIcon(taskbarIcon_);
        taskbarIcon_ = newIcon;
    }
}

// ─── Background with gradient + particles ───────────────────────────────────

void App::DrawBackground() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 tl = vp->WorkPos;
    const ImVec2 br = ImVec2(tl.x + vp->WorkSize.x, tl.y + vp->WorkSize.y);

    // Main gradient: medium navy → vibrant purple → rich magenta
    const ImU32 colTop = IM_COL32(35, 32, 72, 255);
    const ImU32 colMid = IM_COL32(68, 42, 128, 255);
    const ImU32 colBot = IM_COL32(105, 55, 148, 255);
    const float midY = tl.y + vp->WorkSize.y * 0.5f;
    DrawGradientRect(dl, tl, ImVec2(br.x, midY), colTop, colMid);
    DrawGradientRect(dl, ImVec2(tl.x, midY), br, colMid, colBot);

    // Subtle diagonal light streak
    const float streakPhase = fmodf(animTime_ * 0.08f, 2.0f);
    if (streakPhase < 1.0f) {
        const float t = streakPhase;
        const float sx = tl.x + vp->WorkSize.x * (t * 1.5f - 0.25f);
        const float w = vp->WorkSize.x * 0.15f;
        dl->AddRectFilledMultiColor(
            ImVec2(sx, tl.y), ImVec2(sx + w, br.y),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 8),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    }

    // Initialize particles
    if (!particlesInited_) {
        particlesInited_ = true;
        particles_.resize(kParticleCount);
        for (auto& p : particles_) {
            p.x = (float)(rand() % (int)vp->WorkSize.x);
            p.y = (float)(rand() % (int)vp->WorkSize.y);
            p.vx = ((rand() % 100) / 100.0f - 0.5f) * 0.3f;
            p.vy = ((rand() % 100) / 100.0f - 0.5f) * 0.2f - 0.1f;
            p.radius = 1.0f + (rand() % 30) / 10.0f;
            p.alpha = 0.2f + (rand() % 60) / 100.0f;
            p.phase = (rand() % 628) / 100.0f;
        }
    }

    // Update & draw particles
    const float dt = ImGui::GetIO().DeltaTime;
    for (auto& p : particles_) {
        p.x += p.vx;
        p.y += p.vy;
        if (p.x < 0) p.x += vp->WorkSize.x;
        if (p.x > vp->WorkSize.x) p.x -= vp->WorkSize.x;
        if (p.y < 0) p.y += vp->WorkSize.y;
        if (p.y > vp->WorkSize.y) p.y -= vp->WorkSize.y;

        const float flicker = 0.6f + 0.4f * sinf(animTime_ * 1.5f + p.phase);
        const int alpha = (int)(p.alpha * flicker * 255.0f);
        dl->AddCircleFilled(
            ImVec2(tl.x + p.x, tl.y + p.y),
            p.radius * UiScale(),
            IM_COL32(180, 160, 255, alpha), 8);
    }

    animTime_ += dt;

    // Update animated taskbar icon
    UpdateTaskbarIcon();
}

// DrawLuaEditorWithLineNumbers — VSCode-like syntax highlighting.
// Architecture:
//   - ImGuiCol_Text is set to alpha=0 so ImGui's native text is invisible.
//   - Syntax-colored text is drawn on GetForegroundDrawList() — the ONLY layer
//     that renders above InputTextMultiline's internal child window.
//   - The completion popup is also drawn manually on the foreground draw list
//     so it appears opaque above the syntax highlighting.
//   - Gutter, line highlights, etc. use the window draw list (they're behind
//     the InputText child, but that's fine since they're background elements).

static void DrawLuaEditorWithLineNumbers(LuaScriptUiState* ui, std::string* text, float height, bool readOnly, int highlightLine, int* lastScrollToLine) {
    if (!text) return;
    const float s = UiScale();
    const float gutter = 70.0f * s;

    // Editor colors
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.12f, 0.16f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.11f, 0.12f, 0.16f, 0.97f));
    // Text alpha=0: ImGui's native text is invisible. We draw colored text on foreground.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    // Selection alpha=0: ImGui's native selection is invisible. We draw our own on foreground
    // to guarantee perfect alignment with our syntax-highlighted text.
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    ImGui::BeginChild("##lua_editor_child", ImVec2(-1, height), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    // Force this wrapper child to never scroll — only the InputTextMultiline's internal child should scroll
    ImGui::SetScrollY(0.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    // CRITICAL: InputTextMultiline internally uses FontSize (GetTextLineHeight) for line spacing,
    // NOT GetTextLineHeightWithSpacing. We must match this exactly for alignment.
    const float lineH = ImGui::GetTextLineHeight();

    int lineCount = 1;
    for (char ch : *text) if (ch == '\n') ++lineCount;

    // InputTextMultiline fills the child and handles its own internal scrolling.
    // We must NOT make this child scrollable — that creates a competing scrollbar.
    ImGui::SetCursorPosX(gutter);
    const ImGuiInputTextFlags ro = readOnly ? ImGuiInputTextFlags_ReadOnly : 0;
    const ImGuiInputTextFlags assistFlags = ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCompletion;

    // When completion popup is open, intercept Up/Down/Enter/Tab BEFORE InputText
    // processes them. We suppress these keys from ImGui's IO so InputText never sees
    // them, then handle them ourselves after the widget call.
    bool popupKeyUp = false, popupKeyDown = false, popupKeyAccept = false;
    const bool popupActive = ui && !readOnly && ui->assistEnabled && ui->completionOpen && !ui->completionMatches.empty();
    if (popupActive) {
        ImGuiIO& io = ImGui::GetIO();
        // Read key states before suppressing
        popupKeyDown = ImGui::IsKeyPressed(ImGuiKey_DownArrow);
        popupKeyUp = ImGui::IsKeyPressed(ImGuiKey_UpArrow);
        popupKeyAccept = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) || ImGui::IsKeyPressed(ImGuiKey_Tab);
        // Suppress these keys so InputText doesn't move the cursor
        if (popupKeyDown || popupKeyUp || popupKeyAccept) {
            // Clear the key events from InputEvents so InputText won't process them.
            // We do this by setting the keys as "not down" temporarily via KeysData.
            auto clearKey = [&](ImGuiKey key) {
                ImGuiKeyData& kd = io.KeysData[key - ImGuiKey_NamedKey_BEGIN];
                kd.Down = false;
                kd.DownDuration = -1.0f;
                kd.DownDurationPrev = -1.0f;
            };
            if (popupKeyDown) clearKey(ImGuiKey_DownArrow);
            if (popupKeyUp) clearKey(ImGuiKey_UpArrow);
            if (popupKeyAccept) {
                clearKey(ImGuiKey_Enter);
                clearKey(ImGuiKey_KeypadEnter);
                clearKey(ImGuiKey_Tab);
            }
        }
    }

    // Always use callback version to track cursor position (needed for line highlight).
    // The assistEnabled flag only controls whether completion suggestions are shown.
    if (!readOnly && ui) {
        InputTextMultilineStringWithCallback("##luaeditor", text, ImVec2(-1, -1), ro | assistFlags, LuaEditorInputCallback, ui);
    } else {
        InputTextMultilineString("##luaeditor", text, ImVec2(-1, -1), ro);
    }

    // Now handle the suppressed popup keys
    if (popupActive && ui) {
        const int maxItems = std::min((int)ui->completionMatches.size(), 50);
        if (popupKeyDown && ui->completionSelected < maxItems - 1)
            ui->completionSelected++;
        if (popupKeyUp && ui->completionSelected > 0)
            ui->completionSelected--;
        if (popupKeyAccept) {
            const auto& docs = LuaEngine::ApiDocs();
            const int sel = std::clamp(ui->completionSelected, 0, maxItems - 1);
            const int di = ui->completionMatches[sel];
            const char* nm = docs[di].name;
            if (nm) ui->completionPendingInsert = nm;
        }
    }
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const bool editorActive = ImGui::IsItemActive();

    // Auto-trigger completion
    if (ui && !readOnly && ui->assistEnabled && editorActive) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space)) ui->completionOpen = true;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ui->completionOpen = false;
        if (!ui->completionOpen && ui->completionPrefix.size() >= 2 && !ui->completionMatches.empty())
            ui->completionOpen = true;
        if (ui->completionOpen && ui->completionMatches.empty())
            ui->completionOpen = false;
    }

    ImDrawList* fg = ImGui::GetForegroundDrawList();
    const ImVec2 winPos = ImGui::GetWindowPos();
    const ImVec2 winSize = ImGui::GetWindowSize();

    const ImVec2 fp = ImGui::GetStyle().FramePadding;
    const float textStartX = itemMin.x + fp.x;
    // InputTextMultiline internally calls BeginChildEx(label, id, ...) which creates a child
    // window named "parentName/##luaeditor_XXXXXXXX" (label + underscore + hex ID).
    // We iterate context windows to find it robustly, matching by suffix.
    float scrollY = 0.0f;
    ImGuiWindow* editorInnerWin = nullptr;
    {
        ImGuiContext& ctx = *ImGui::GetCurrentContext();
        for (int i = 0; i < ctx.Windows.Size; ++i) {
            ImGuiWindow* w = ctx.Windows[i];
            if (w && w->Name && strstr(w->Name, "/##luaeditor_")) {
                editorInnerWin = w;
                scrollY = w->Scroll.y;
                break;
            }
        }
    }
    const float textStartY = itemMin.y + fp.y - scrollY;

    // Compute cursor line (used by highlight + popup)
    int cursorLine = 0;
    if (ui) {
        const int cp = std::clamp(ui->completionCursorPos, 0, (int)text->size());
        for (int ci = 0; ci < cp; ++ci) if ((*text)[ci] == '\n') ++cursorLine;
    }

    const int first = std::max(0, (int)(scrollY / lineH));
    const int visible = (int)(winSize.y / lineH) + 3;
    const int last = std::min(lineCount, first + visible);

    // Compute completion popup rect early (needed for clipping)
    const bool showPopup = ui && !readOnly && ui->assistEnabled && editorActive
                           && ui->completionOpen && !ui->completionMatches.empty();
    const float popupW = 420.0f * s;
    // Compute popup height dynamically to fit all content:
    // header + separator + list + separator + signature + brief + hint + padding
    const float popupFontSz = ImGui::GetFontSize();
    const float popupPad = 8.0f * s;
    const float popupListH = 110.0f * s;
    const float popupH = popupPad                          // top padding
                        + popupFontSz + 6.0f * s           // header line
                        + 4.0f * s                          // separator + gap
                        + popupListH                        // list area
                        + 4.0f * s                          // separator + gap
                        + popupFontSz + 2.0f * s            // signature
                        + popupFontSz + 4.0f * s            // brief
                        + popupFontSz                       // hint line
                        + popupPad;                         // bottom padding
    float popupX = 0, popupY = 0;
    if (showPopup) {
        popupY = textStartY + (cursorLine + 1) * lineH + 2.0f * s;
        if (popupY + popupH > winPos.y + winSize.y)
            popupY = textStartY + cursorLine * lineH - popupH - 2.0f * s;
        popupY = std::clamp(popupY, winPos.y, winPos.y + winSize.y - popupH);
        popupX = winPos.x + gutter + 6.0f * s;
    }

    // ── Foreground draw list — the ONLY layer above InputTextMultiline's child ──
    fg->PushClipRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), true);

    // Gutter background (on foreground so it's visible)
    fg->AddRectFilled(ImVec2(winPos.x, winPos.y), ImVec2(winPos.x + gutter - 4.0f * s, winPos.y + winSize.y),
        IM_COL32(22, 24, 34, 240));
    fg->AddLine(ImVec2(winPos.x + gutter - 4.0f * s, winPos.y),
                ImVec2(winPos.x + gutter - 4.0f * s, winPos.y + winSize.y),
                IM_COL32(60, 55, 90, 150), 1.0f);

    // Vertical padding for highlight rects: text glyphs have ascent/descent so the
    // visual center sits slightly above the mathematical center of [y, y+lineH].
    // Extending the rect downward by a small amount makes text look vertically centered.
    const float hlPadBot = lineH * 0.15f;

    // Current cursor line highlight
    if (ui && editorActive && !readOnly) {
        const float cy = textStartY + cursorLine * lineH;
        fg->AddRectFilled(
            ImVec2(winPos.x + gutter - 4.0f * s, cy),
            ImVec2(winPos.x + winSize.x, cy + lineH + hlPadBot),
            IM_COL32(70, 65, 120, 60));
    }

    // Execution line highlight
    if (highlightLine > 0) {
        const float y = textStartY + (highlightLine - 1) * lineH;
        fg->AddRectFilled(ImVec2(winPos.x, y), ImVec2(winPos.x + winSize.x, y + lineH + hlPadBot), IM_COL32(100, 80, 255, 50));
    }

    // Selection highlight — drawn on foreground to align perfectly with our syntax text
    if (ui && editorActive && ui->selectionStart != ui->selectionEnd) {
        ImFont* selFont = ImGui::GetFont();
        const float selFontSz = ImGui::GetFontSize();
        const int selMin = std::min(ui->selectionStart, ui->selectionEnd);
        const int selMax = std::max(ui->selectionStart, ui->selectionEnd);
        const std::string& src = *text;
        const int srcLen = (int)src.size();

        // Walk through lines and draw selection rects for each affected line
        int charIdx = 0, lineIdx = 0;
        int lineStartIdx = 0;
        while (charIdx <= srcLen) {
            const bool isEnd = (charIdx == srcLen) || (src[charIdx] == '\n');
            if (isEnd) {
                const int lineEndIdx = charIdx;
                // Check if this line overlaps with selection
                if (lineIdx >= first && lineIdx < last && lineStartIdx < selMax && lineEndIdx >= selMin) {
                    const int hlStart = std::max(selMin, lineStartIdx);
                    const int hlEnd = std::min(selMax, lineEndIdx);
                    // Compute X positions
                    float xStart = textStartX;
                    if (hlStart > lineStartIdx)
                        xStart += selFont->CalcTextSizeA(selFontSz, FLT_MAX, 0.0f, src.data() + lineStartIdx, src.data() + hlStart).x;
                    float xEnd = textStartX;
                    if (hlEnd > lineStartIdx)
                        xEnd += selFont->CalcTextSizeA(selFontSz, FLT_MAX, 0.0f, src.data() + lineStartIdx, src.data() + hlEnd).x;
                    // If selection extends to end of line (or beyond), extend to edge
                    if (selMax > lineEndIdx)
                        xEnd = std::max(xEnd + selFontSz * 0.5f, xEnd); // small extension for newline
                    const float ly = textStartY + lineIdx * lineH;
                    fg->AddRectFilled(ImVec2(xStart, ly), ImVec2(xEnd, ly + lineH + hlPadBot),
                        IM_COL32(56, 84, 153, 130));
                }
                lineIdx++;
                lineStartIdx = charIdx + 1;
                if (lineIdx >= last) break;
            }
            charIdx++;
        }
    }

    // Line numbers
    char lnBuf[16]{};
    for (int i = first; i < last; ++i) {
        std::snprintf(lnBuf, sizeof(lnBuf), "%05d", i + 1);
        fg->AddText(ImVec2(origin.x + 6.0f * s, textStartY + i * lineH), IM_COL32(100, 110, 140, 220), lnBuf);
    }

    // ── Syntax highlighting ──
    // Strategy: for each visible token, draw a background-colored rect to cover
    // ImGui's gray fallback text, then draw the colored token on top.
    // This way text is ALWAYS visible — colored if coords are right, gray if not.
    {
        const auto& docs = LuaEngine::ApiDocs();
        auto isIdent = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };
        auto isApi = [&docs](std::string_view tok) {
            for (auto& d : docs) { if (!d.name) continue; if (tok.size() == std::strlen(d.name) && std::memcmp(d.name, tok.data(), tok.size()) == 0) return true; } return false;
        };
        auto isKw = [](std::string_view tok) {
            static const char* k[] = {"and","break","do","else","elseif","end","false","for","function","goto","if","in","local","nil","not","or","repeat","return","then","true","until","while"};
            for (auto kw : k) if (tok.size() == std::strlen(kw) && std::memcmp(kw, tok.data(), tok.size()) == 0) return true; return false;
        };
        auto isBuiltin = [](std::string_view tok) {
            static const char* b[] = {"print","type","tostring","tonumber","pairs","ipairs","next","select","unpack","require","error","assert","pcall","xpcall","rawget","rawset","rawlen","rawequal","setmetatable","getmetatable","table","string","math","io","os","coroutine","debug","utf8"};
            for (auto bi : b) if (tok.size() == std::strlen(bi) && std::memcmp(bi, tok.data(), tok.size()) == 0) return true; return false;
        };
        auto isConst = [](std::string_view tok) { return tok == "true" || tok == "false" || tok == "nil"; };

        // One Dark Pro palette (matching the screenshot reference)
        const ImU32 kwCol    = IM_COL32(198, 120, 221, 255); // purple — keywords
        const ImU32 fnCol    = IM_COL32(97, 175, 239, 255);  // blue — functions
        const ImU32 builtCol = IM_COL32(229, 192, 123, 255); // gold — builtins
        const ImU32 cmCol    = IM_COL32(106, 115, 130, 255); // gray — comments
        const ImU32 numCol   = IM_COL32(209, 154, 102, 255); // orange — numbers
        const ImU32 strCol   = IM_COL32(152, 195, 121, 255); // green — strings
        const ImU32 constCol = IM_COL32(86, 182, 194, 255);  // cyan — constants
        const ImU32 opCol    = IM_COL32(190, 195, 210, 255); // light — operators
        const ImU32 defCol   = IM_COL32(210, 214, 224, 255); // white — identifiers
        const ImU32 parenCol = IM_COL32(220, 180, 100, 255); // warm — brackets

        ImFont* font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize();

        auto drawToken = [&](float x, float y, ImU32 col, const char* begin, const char* end) -> float {
            const ImVec2 sz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, begin, end);
            fg->AddText(ImVec2(x, y), col, begin, end);
            return sz.x;
        };

        const std::string& src = *text;
        int line = 0, lineStart = 0;
        for (int idx = 0; idx <= (int)src.size(); ++idx) {
            if (idx < (int)src.size() && src[idx] != '\n') continue;
            if (line >= first && line < last) {
                const char* lp = src.data() + lineStart;
                const int llen = idx - lineStart;
                const float baseY = textStartY + line * lineH;
                float cx = textStartX;

                int i = 0;
                while (i < llen) {
                    // Comment
                    if (i + 1 < llen && lp[i] == '-' && lp[i + 1] == '-') {
                        cx += drawToken(cx, baseY, cmCol, lp + i, lp + llen);
                        i = llen; break;
                    }
                    // String
                    if (lp[i] == '\'' || lp[i] == '"') {
                        char q = lp[i]; int k = i + 1;
                        while (k < llen && lp[k] != q) { if (lp[k] == '\\' && k + 1 < llen) k += 2; else ++k; }
                        if (k < llen) ++k;
                        cx += drawToken(cx, baseY, strCol, lp + i, lp + k);
                        i = k; continue;
                    }
                    // Number
                    if (std::isdigit((unsigned char)lp[i]) || (lp[i] == '.' && i + 1 < llen && std::isdigit((unsigned char)lp[i + 1]))) {
                        int k = i;
                        if (lp[k] == '0' && k + 1 < llen && (lp[k + 1] == 'x' || lp[k + 1] == 'X')) { k += 2; while (k < llen && std::isxdigit((unsigned char)lp[k])) ++k; }
                        else { while (k < llen && (std::isdigit((unsigned char)lp[k]) || lp[k] == '.')) ++k;
                            if (k < llen && (lp[k] == 'e' || lp[k] == 'E')) { ++k; if (k < llen && (lp[k] == '+' || lp[k] == '-')) ++k; while (k < llen && std::isdigit((unsigned char)lp[k])) ++k; } }
                        cx += drawToken(cx, baseY, numCol, lp + i, lp + k);
                        i = k; continue;
                    }
                    // Identifier
                    if (isIdent(lp[i])) {
                        int k = i + 1; while (k < llen && isIdent(lp[k])) ++k;
                        std::string_view tok(lp + i, k - i);
                        ImU32 col;
                        if (isConst(tok))        col = constCol;
                        else if (isKw(tok))      col = kwCol;
                        else if (isApi(tok))     col = fnCol;
                        else if (isBuiltin(tok)) col = builtCol;
                        else { int p = k; while (p < llen && (lp[p] == ' ' || lp[p] == '\t')) ++p;
                               col = (p < llen && lp[p] == '(') ? fnCol : defCol; }
                        cx += drawToken(cx, baseY, col, lp + i, lp + k);
                        i = k; continue;
                    }
                    // Brackets
                    if (lp[i] == '(' || lp[i] == ')' || lp[i] == '[' || lp[i] == ']' || lp[i] == '{' || lp[i] == '}') {
                        cx += drawToken(cx, baseY, parenCol, lp + i, lp + i + 1);
                        ++i; continue;
                    }
                    // Whitespace — just advance, don't cover
                    if (lp[i] == ' ' || lp[i] == '\t') {
                        cx += font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, lp + i, lp + i + 1).x;
                        ++i; continue;
                    }
                    // Operators / other
                    cx += drawToken(cx, baseY, opCol, lp + i, lp + i + 1);
                    ++i;
                }
            }
            ++line; lineStart = idx + 1;
            if (line >= last) break;
        }
    }
    fg->PopClipRect();

    // ── Completion popup — drawn on foreground draw list (opaque, above syntax) ──
    if (showPopup) {
        const auto& docs = LuaEngine::ApiDocs();
        ImFont* pfont = ImGui::GetFont();
        const float pfontSz = ImGui::GetFontSize();
        const float pad = 8.0f * s;
        const float itemH = 22.0f * s;

        const ImVec2 pMin(popupX, popupY);
        const ImVec2 pMax(popupX + popupW, popupY + popupH);

        // Draw popup on foreground so it's above syntax highlighting
        fg->PushClipRect(pMin, pMax, true);

        // Opaque background + border
        fg->AddRectFilled(pMin, pMax, IM_COL32(33, 36, 46, 252), 6.0f * s);
        fg->AddRect(pMin, pMax, IM_COL32(77, 82, 115, 200), 6.0f * s, 0, 1.0f);

        float cy = popupY + pad;

        // Header
        {
            char hdr[128];
            std::snprintf(hdr, sizeof(hdr), "补全 %s", ui->completionPrefix.c_str());
            fg->AddText(ImVec2(popupX + pad, cy), IM_COL32(140, 153, 191, 255), hdr);
            char cnt[32];
            std::snprintf(cnt, sizeof(cnt), "%d 项", (int)ui->completionMatches.size());
            const ImVec2 cntSz = pfont->CalcTextSizeA(pfontSz, FLT_MAX, 0.0f, cnt);
            fg->AddText(ImVec2(pMax.x - pad - cntSz.x, cy), IM_COL32(115, 122, 148, 200), cnt);
            cy += pfontSz + 6.0f * s;
        }

        // Separator
        fg->AddLine(ImVec2(popupX + pad, cy), ImVec2(pMax.x - pad, cy), IM_COL32(77, 82, 115, 150), 1.0f);
        cy += 4.0f * s;

        // List items with mouse interaction
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        const bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const int maxItems = std::min((int)ui->completionMatches.size(), 50);
        const float listH = 110.0f * s;
        const int visibleItems = std::min(maxItems, (int)(listH / itemH));

        static int scrollOff = 0;
        if (ui->completionSelected < scrollOff) scrollOff = ui->completionSelected;
        if (ui->completionSelected >= scrollOff + visibleItems) scrollOff = ui->completionSelected - visibleItems + 1;
        scrollOff = std::clamp(scrollOff, 0, std::max(0, maxItems - visibleItems));

        for (int vi = 0; vi < visibleItems && (scrollOff + vi) < maxItems; ++vi) {
            const int k = scrollOff + vi;
            const int di = ui->completionMatches[k];
            const bool sel = (ui->completionSelected == k);
            const float iy = cy + vi * itemH;
            const ImVec2 iMin(popupX + pad, iy);
            const ImVec2 iMax(pMax.x - pad, iy + itemH);

            const bool hovered = mousePos.x >= iMin.x && mousePos.x < iMax.x
                              && mousePos.y >= iMin.y && mousePos.y < iMax.y;
            if (hovered && mouseClicked) {
                ui->completionSelected = k;
                ui->completionPendingInsert = docs[di].name ? docs[di].name : "";
            }

            if (sel)
                fg->AddRectFilled(iMin, iMax, IM_COL32(51, 77, 140, 153), 3.0f * s);
            else if (hovered)
                fg->AddRectFilled(iMin, iMax, IM_COL32(64, 89, 153, 100), 3.0f * s);

            const char* nm = docs[di].name ? docs[di].name : "";
            const char* gr = docs[di].group ? docs[di].group : "";
            fg->AddText(ImVec2(iMin.x + 4.0f * s, iy + (itemH - pfontSz) * 0.5f),
                        IM_COL32(128, 140, 179, 200), gr);
            fg->AddText(ImVec2(iMin.x + 66.0f * s, iy + (itemH - pfontSz) * 0.5f),
                        IM_COL32(153, 204, 242, 255), nm);
            if (docs[di].signature) {
                const ImVec2 nmSz = pfont->CalcTextSizeA(pfontSz, FLT_MAX, 0.0f, nm);
                fg->AddText(ImVec2(iMin.x + 66.0f * s + nmSz.x + 6.0f * s, iy + (itemH - pfontSz) * 0.5f),
                            IM_COL32(128, 133, 158, 179), docs[di].signature);
            }
        }
        cy += listH;

        // Separator
        fg->AddLine(ImVec2(popupX + pad, cy), ImVec2(pMax.x - pad, cy), IM_COL32(77, 82, 115, 150), 1.0f);
        cy += 4.0f * s;

        // Detail area
        const int si = std::clamp(ui->completionSelected, 0, (int)ui->completionMatches.size() - 1);
        const int detailIdx = ui->completionMatches[si];
        if (docs[detailIdx].signature)
            fg->AddText(ImVec2(popupX + pad, cy), IM_COL32(179, 204, 242, 255), docs[detailIdx].signature);
        cy += pfontSz + 2.0f * s;
        if (docs[detailIdx].brief)
            fg->AddText(ImVec2(popupX + pad, cy), IM_COL32(166, 173, 199, 230), docs[detailIdx].brief);
        cy += pfontSz + 4.0f * s;
        fg->AddText(ImVec2(popupX + pad, cy), IM_COL32(115, 122, 148, 153), "Up/Down | Enter | Esc");

        fg->PopClipRect();

        // Keyboard navigation is handled in LuaEditorInputCallback to prevent
        // InputText from consuming Up/Down/Enter keys before we can intercept them.
    }

    if (highlightLine > 0 && lastScrollToLine && *lastScrollToLine != highlightLine) {
        if (editorInnerWin) {
            float targetY = (highlightLine - 1) * lineH;
            float viewH = editorInnerWin->InnerRect.GetHeight();
            editorInnerWin->Scroll.y = std::max(0.0f, targetY - viewH * 0.35f);
        }
        *lastScrollToLine = highlightLine;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(4);
}

// ─── String helpers ─────────────────────────────────────────────────────────

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out; out.resize((size_t)len);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len);
    return out;
}
static std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out; out.resize((size_t)len);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len, nullptr, nullptr);
    return out;
}

// ─── ImGui InputText string adapters ────────────────────────────────────────

static int ImGuiStringResizeCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) return 0;
    auto* str = static_cast<std::string*>(data->UserData);
    str->resize((size_t)data->BufTextLen); data->Buf = str->data(); return 0;
}
static bool InputTextString(const char* label, std::string* str, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    if (str->capacity() < 256) str->reserve(256); str->resize(str->capacity());
    const bool changed = ImGui::InputText(label, str->data(), str->size() + 1, flags, ImGuiStringResizeCallback, str);
    str->resize(std::strlen(str->c_str())); return changed;
}
static bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags extraFlags = 0) {
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize | extraFlags;
    if (str->capacity() < 4096) str->reserve(4096); str->resize(str->capacity());
    const bool changed = ImGui::InputTextMultiline(label, str->data(), str->size() + 1, size, flags, ImGuiStringResizeCallback, str);
    str->resize(std::strlen(str->c_str())); return changed;
}
struct InputTextChainCtx { std::string* str; ImGuiInputTextCallback callback; void* userData; };
static int ImGuiStringResizeChainCallback(ImGuiInputTextCallbackData* data) {
    auto* ctx = static_cast<InputTextChainCtx*>(data->UserData);
    if (!ctx || !ctx->str) return 0;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) { ctx->str->resize((size_t)data->BufTextLen); data->Buf = ctx->str->data(); return 0; }
    if (!ctx->callback) return 0;
    void* old = data->UserData; data->UserData = ctx->userData; const int r = ctx->callback(data); data->UserData = old; return r;
}
static bool InputTextMultilineStringWithCallback(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags extraFlags, ImGuiInputTextCallback callback, void* userData) {
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize | extraFlags;
    if (str->capacity() < 4096) str->reserve(4096); str->resize(str->capacity());
    InputTextChainCtx ctx{ str, callback, userData };
    const bool changed = ImGui::InputTextMultiline(label, str->data(), str->size() + 1, size, flags, ImGuiStringResizeChainCallback, &ctx);
    str->resize(std::strlen(str->c_str())); return changed;
}

// ─── Lua completion helpers ─────────────────────────────────────────────────

static bool ContainsCaseInsensitive(const char* s, const std::string& needle) {
    if (!s) return false; if (needle.empty()) return true;
    for (const char* p = s; *p; ++p) { size_t i = 0; while (i < needle.size() && p[i] && std::tolower((unsigned char)p[i]) == std::tolower((unsigned char)needle[i])) ++i; if (i == needle.size()) return true; } return false;
}
static bool IsIdentChar(char c) { return std::isalnum((unsigned char)c) != 0 || c == '_'; }

static void BuildCompletionMatches(LuaScriptUiState* ui, const char* buf, int cursorPos) {
    if (!ui || !buf) return;
    const int len = (int)std::strlen(buf); const int cur = std::clamp(cursorPos, 0, len);
    int start = cur; while (start > 0 && IsIdentChar(buf[start - 1])) --start;
    ui->completionCursorPos = cur; ui->completionWordStart = start;
    ui->completionPrefix.assign(buf + start, buf + cur);
    ui->completionMatches.clear(); if (!ui->assistEnabled || ui->completionPrefix.empty()) return;
    const auto& docs = LuaEngine::ApiDocs(); ui->completionMatches.reserve(32);
    for (int i = 0; i < (int)docs.size(); ++i) { if (!docs[i].name) continue; if (std::strncmp(docs[i].name, ui->completionPrefix.c_str(), ui->completionPrefix.size()) == 0) ui->completionMatches.push_back(i); }
    if (ui->completionSelected >= (int)ui->completionMatches.size()) ui->completionSelected = 0;
}
static std::string CommonPrefix(const LuaScriptUiState* ui) {
    if (!ui || ui->completionMatches.empty()) return {};
    const auto& docs = LuaEngine::ApiDocs();
    std::string p = docs[ui->completionMatches[0]].name ? docs[ui->completionMatches[0]].name : "";
    for (size_t i = 1; i < ui->completionMatches.size(); ++i) { const char* s = docs[ui->completionMatches[i]].name ? docs[ui->completionMatches[i]].name : ""; size_t j = 0; while (j < p.size() && s[j] && p[j] == s[j]) ++j; p.resize(j); if (p.empty()) break; }
    return p;
}
static int LuaEditorInputCallback(ImGuiInputTextCallbackData* data) {
    auto* ui = static_cast<LuaScriptUiState*>(data->UserData); if (!ui) return 0;
    // Process pending completion insert FIRST, before cursor position updates.
    // This is critical because clicking the popup moves the cursor, which would
    // corrupt completionWordStart/completionCursorPos if we updated them first.
    if (!ui->completionPendingInsert.empty()) {
        const std::string insert = ui->completionPendingInsert; ui->completionPendingInsert.clear();
        const int start = std::clamp(ui->completionWordStart, 0, data->BufTextLen);
        const int cur = std::clamp(ui->completionCursorPos, 0, data->BufTextLen);
        if (cur >= start) { data->DeleteChars(start, cur - start); data->InsertChars(start, insert.c_str()); data->CursorPos = start + (int)insert.size(); }
        ui->completionOpen = false;
        // Update tracking after insert
        ui->completionCursorPos = data->CursorPos;
        ui->selectionStart = data->CursorPos;
        ui->selectionEnd = data->CursorPos;
        return 0;
    }
    // Track cursor and selection position (needed for line highlight + selection rendering)
    // Note: keyboard navigation for the completion popup is handled BEFORE InputText
    // in DrawLuaEditorWithLineNumbers by suppressing keys from ImGui IO.
    ui->completionCursorPos = data->CursorPos;
    ui->selectionStart = data->HasSelection() ? data->SelectionStart : data->CursorPos;
    ui->selectionEnd = data->HasSelection() ? data->SelectionEnd : data->CursorPos;
    if (data->Buf) BuildCompletionMatches(ui, data->Buf, data->CursorPos);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        if (data->Buf) BuildCompletionMatches(ui, data->Buf, data->CursorPos);
        if (ui->completionMatches.empty()) return 0;
        const auto& docs = LuaEngine::ApiDocs();
        const int start = std::clamp(ui->completionWordStart, 0, data->BufTextLen);
        const int cur = std::clamp(ui->completionCursorPos, 0, data->BufTextLen);
        if (ui->completionMatches.size() == 1) { const char* name = docs[ui->completionMatches[0]].name; if (!name) return 0; data->DeleteChars(start, cur - start); data->InsertChars(start, name); data->CursorPos = start + (int)std::strlen(name); ui->completionOpen = false; return 0; }
        const std::string common = CommonPrefix(ui);
        if (!common.empty() && common.size() > ui->completionPrefix.size()) { data->DeleteChars(start, cur - start); data->InsertChars(start, common.c_str()); data->CursorPos = start + (int)common.size(); ui->completionOpen = true; return 0; }
        ui->completionOpen = true; return 0;
    }
    return 0;
}

// ─── Lua docs panel ─────────────────────────────────────────────────────────

static void DrawLuaDocsPanel(LuaScriptUiState* ui, float height, bool disabled) {
    if (!ui) return;
    const auto& docs = LuaEngine::ApiDocs();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.08f, 0.20f, 0.70f));
    ImGui::BeginChild("##lua_docs_panel", ImVec2(-1, height), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.75f, 0.95f, 1.0f));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Lua API");
    ImGui::PopStyleColor();
    ImGui::SameLine(); ImGui::Checkbox("提示/补全", &ui->assistEnabled);
    if (disabled) ImGui::BeginDisabled();
    InputTextString("搜索##lua_docs", &ui->docsFilter);
    if (disabled) ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::BeginChild("##lua_docs_list", ImVec2(-1, height * 0.55f), true);
    for (int i = 0; i < (int)docs.size(); ++i) {
        const auto& d = docs[i];
        const bool pass = ui->docsFilter.empty() || ContainsCaseInsensitive(d.name, ui->docsFilter) || ContainsCaseInsensitive(d.signature, ui->docsFilter) || ContainsCaseInsensitive(d.group, ui->docsFilter);
        if (!pass) continue;
        const bool selected = (ui->docsSelected == i);
        std::string label = (d.group ? std::string(d.group) : std::string()) + "  " + (d.name ? d.name : "");
        if (ImGui::Selectable(label.c_str(), selected)) ui->docsSelected = i;
    }
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::BeginChild("##lua_docs_detail", ImVec2(-1, 0), true);
    const int sel = ui->docsSelected;
    if (sel >= 0 && sel < (int)docs.size()) {
        const auto& d = docs[sel];
        if (d.name) ImGui::Text("%s", d.name);
        if (d.signature) ImGui::TextDisabled("%s", d.signature);
        if (d.brief) ImGui::TextWrapped("%s", d.brief);
        if (ImGui::Button("复制签名") && d.signature) ImGui::SetClipboardText(d.signature);
    } else { ImGui::TextDisabled("选择一个函数查看说明"); }
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─── App lifecycle ──────────────────────────────────────────────────────────

App::App(HINSTANCE hInstance, HWND hwnd) : hInstance_(hInstance), hwnd_(hwnd) {
    overlay_.Create(hInstance_);
    lua_.Init(&replayer_);
    luaEditor_ = "set_speed(1.0)\nmouse_move(500, 500)\nmouse_down('left')\nwait_ms(60)\nmouse_up('left')\n";
    LoadConfig();
}
App::~App() { SaveConfig(); EmergencyStop(); lua_.Shutdown(); overlay_.Destroy(); }
void App::OnHotkey() { EmergencyStop(); }

// ─── Main frame ─────────────────────────────────────────────────────────────

void App::OnFrame() {
    const float s = UiScale();
    ImGuiStyle& style = ImGui::GetStyle();

    // Dark theme style overrides for glass UI
    style.WindowPadding = ImVec2(14.0f * s, 12.0f * s);
    style.FramePadding = ImVec2(10.0f * s, 6.0f * s);
    style.ItemSpacing = ImVec2(10.0f * s, 8.0f * s);
    style.ItemInnerSpacing = ImVec2(6.0f * s, 4.0f * s);
    style.WindowRounding = 0.0f;
    style.FrameRounding = 6.0f * s;
    style.GrabRounding = 6.0f * s;
    style.ScrollbarRounding = 8.0f * s;
    style.ChildRounding = 10.0f * s;
    style.PopupRounding = 8.0f * s;

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

    // Draw animated background BEFORE the main window
    DrawBackground();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); // transparent - we draw our own bg
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0, 0, 0, 0));
    ImGui::Begin("AutoClicker-Pro", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor(5);
    // Force main window scroll to zero — prevents any accidental content overflow from showing a scrollbar
    ImGui::SetScrollY(0.0f);

    // ═══════════════════════════════════════════════════════════════════════
    // HEADER BAR - gradient glass
    // ═══════════════════════════════════════════════════════════════════════
    {
        const float headerH = 50.0f * s;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.11f, 0.26f, 0.75f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        ImGui::BeginChild("##header", ImVec2(0, headerH), false, ImGuiWindowFlags_NoScrollbar);

        // Gradient overlay on header
        ImDrawList* hdl = ImGui::GetWindowDrawList();
        const ImVec2 hp = ImGui::GetWindowPos();
        const ImVec2 hs = ImGui::GetWindowSize();
        hdl->AddRectFilledMultiColor(hp, ImVec2(hp.x + hs.x, hp.y + hs.y),
            IM_COL32(45, 35, 100, 190), IM_COL32(95, 45, 140, 190),
            IM_COL32(95, 45, 140, 170), IM_COL32(45, 35, 100, 170));
        // Bottom glow line
        hdl->AddRectFilledMultiColor(
            ImVec2(hp.x, hp.y + hs.y - 2.0f * s), ImVec2(hp.x + hs.x, hp.y + hs.y),
            IM_COL32(100, 80, 255, 100), IM_COL32(200, 80, 200, 100),
            IM_COL32(200, 80, 200, 0), IM_COL32(100, 80, 255, 0));

        // Title (no logo icon — clean text only)
        ImGui::SetCursorPos(ImVec2(16.0f * s, (headerH - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.87f, 1.0f, 1.0f));
        ImGui::Text("AutoClicker-Pro");
        ImGui::PopStyleColor();

        // Mode tabs - centered pill buttons (absolute positioning for precise centering)
        const float tabBtnW = 100.0f * s;
        const float tabGap = 6.0f * s;
        const float tabTotalW = tabBtnW * 2.0f + tabGap;
        // Calculate absolute screen X for centering within the header
        const float tabStartScreenX = hp.x + (hs.x - tabTotalW) * 0.5f;
        const float tabScreenY = hp.y + (headerH - 30.0f * s) * 0.5f;

        // Tab: 录制回放
        {
            const ImVec2 tabPos(tabStartScreenX, tabScreenY);
            const ImVec2 tabSz(tabBtnW, 30.0f * s);
            ImGui::SetCursorScreenPos(tabPos);
            ImGui::InvisibleButton("##tab0", tabSz);
            if (ImGui::IsItemClicked()) mode_ = 0;
            const bool hov = ImGui::IsItemHovered();
            if (mode_ == 0) {
                hdl->AddRectFilled(tabPos, ImVec2(tabPos.x + tabSz.x, tabPos.y + tabSz.y),
                    IM_COL32(100, 80, 220, 200), 15.0f * s);
                hdl->AddRect(tabPos, ImVec2(tabPos.x + tabSz.x, tabPos.y + tabSz.y),
                    IM_COL32(160, 140, 255, 120), 15.0f * s, 0, 1.0f);
            } else {
                hdl->AddRectFilled(tabPos, ImVec2(tabPos.x + tabSz.x, tabPos.y + tabSz.y),
                    IM_COL32(60, 50, 100, hov ? 150 : 80), 15.0f * s);
            }
            const ImVec2 txt = ImGui::CalcTextSize("录制回放");
            hdl->AddText(ImVec2(tabPos.x + (tabSz.x - txt.x) * 0.5f, tabPos.y + (tabSz.y - txt.y) * 0.5f),
                mode_ == 0 ? IM_COL32(255, 255, 255, 240) : IM_COL32(180, 170, 210, 200), "录制回放");
        }

        ImGui::SameLine(0, tabGap);

        // Tab: Lua 脚本
        {
            const ImVec2 tabPos(tabStartScreenX + tabBtnW + tabGap, tabScreenY);
            const ImVec2 tabSz(tabBtnW, 30.0f * s);
            ImGui::SetCursorScreenPos(tabPos);
            ImGui::InvisibleButton("##tab1", tabSz);
            if (ImGui::IsItemClicked()) mode_ = 1;
            const bool hov = ImGui::IsItemHovered();
            if (mode_ == 1) {
                hdl->AddRectFilled(tabPos, ImVec2(tabPos.x + tabSz.x, tabPos.y + tabSz.y),
                    IM_COL32(100, 80, 220, 200), 15.0f * s);
                hdl->AddRect(tabPos, ImVec2(tabPos.x + tabSz.x, tabPos.y + tabSz.y),
                    IM_COL32(160, 140, 255, 120), 15.0f * s, 0, 1.0f);
            } else {
                hdl->AddRectFilled(tabPos, ImVec2(tabPos.x + tabSz.x, tabPos.y + tabSz.y),
                    IM_COL32(60, 50, 100, hov ? 150 : 80), 15.0f * s);
            }
            const ImVec2 txt = ImGui::CalcTextSize("Lua 脚本");
            hdl->AddText(ImVec2(tabPos.x + (tabSz.x - txt.x) * 0.5f, tabPos.y + (tabSz.y - txt.y) * 0.5f),
                mode_ == 1 ? IM_COL32(255, 255, 255, 240) : IM_COL32(180, 170, 210, 200), "Lua 脚本");
        }

        // Emergency stop - right
        ImGui::SameLine();
        const float hintW = ImGui::CalcTextSize("Ctrl+F12 紧急停止").x + 20.0f * s;
        ImGui::SetCursorPosX(hs.x - hintW);
        ImGui::SetCursorPosY((headerH - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 0.7f), "Ctrl+F12 紧急停止");

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    if (mode_ == 0) DrawSimpleMode();
    if (mode_ == 1) DrawAdvancedMode();

    DrawStatusBar();
    DrawBlockInputConfirmModal();
    DrawExitConfirmModal();
    ImGui::End();

    if (!lua_.IsRunning() && scriptMinimized_ && hwnd_) {
        ShowWindow(hwnd_, SW_RESTORE); SetForegroundWindow(hwnd_); scriptMinimized_ = false;
    }
}

// ─── Event formatting ───────────────────────────────────────────────────────

static std::string FormatEvent(const trc::RawEvent& e) {
    char buf[128];
    const auto type = static_cast<trc::EventType>(e.type);
    switch (type) {
    case trc::EventType::MouseMove:  snprintf(buf, sizeof(buf), "Move (%d, %d)", e.x, e.y); break;
    case trc::EventType::MouseDown:  snprintf(buf, sizeof(buf), "Down %s (%d, %d)", e.data == 0 ? "L" : (e.data == 1 ? "R" : "M"), e.x, e.y); break;
    case trc::EventType::MouseUp:    snprintf(buf, sizeof(buf), "Up %s (%d, %d)", e.data == 0 ? "L" : (e.data == 1 ? "R" : "M"), e.x, e.y); break;
    case trc::EventType::Wheel:      snprintf(buf, sizeof(buf), "Wheel %d", e.data); break;
    case trc::EventType::KeyDown:    snprintf(buf, sizeof(buf), "Key Down 0x%02X", e.data); break;
    case trc::EventType::KeyUp:      snprintf(buf, sizeof(buf), "Key Up 0x%02X", e.data); break;
    default: snprintf(buf, sizeof(buf), "Unknown (%d)", e.type); break;
    }
    if (e.timeDelta > 1000) { std::string str(buf); char t[32]; snprintf(t, sizeof(t), " (+%lld ms)", e.timeDelta / 1000); return str + t; }
    return std::string(buf);
}

// ═══════════════════════════════════════════════════════════════════════════
// SIMPLE MODE
// ═══════════════════════════════════════════════════════════════════════════

void App::DrawSimpleMode() {
    editorRectValid_ = false;  // No editor in simple mode
    const float s = UiScale();
    const float availW = ImGui::GetContentRegionAvail().x;
    const float availH = ImGui::GetContentRegionAvail().y - 40.0f * s;
    const float gap = 10.0f * s;
    const float leftW = 290.0f * s;
    const float rightW = availW - leftW - gap;

    // ─── LEFT COLUMN ────────────────────────────────────────────────────
    ImGui::BeginChild("##simple_left", ImVec2(leftW, availH), false);

    // File card
    BeginGlassCard("##file_card", "文件", ImVec2(0, 0));
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.06f, 0.18f, 0.60f));
        ImGui::SetNextItemWidth(-1);
        InputTextString("##path", &trcPath_);
        ImGui::PopStyleColor();

        const float btnW = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
        if (ImGui::Button("浏览...", ImVec2(btnW, 0))) {
            wchar_t buf[MAX_PATH]{}; std::wstring w = Utf8ToWide(trcPath_); wcsncpy_s(buf, w.c_str(), _TRUNCATE);
            if (OpenFileDialog(nullptr, buf, MAX_PATH, L"Trace File (*.trc)\0*.trc\0\0")) trcPath_ = WideToUtf8(std::wstring(buf));
        }
        ImGui::SameLine();
        if (ImGui::Button("另存为", ImVec2(btnW, 0))) {
            wchar_t buf[MAX_PATH]{}; std::wstring w = Utf8ToWide(trcPath_); wcsncpy_s(buf, w.c_str(), _TRUNCATE);
            if (SaveFileDialog(nullptr, buf, MAX_PATH, L"Trace File (*.trc)\0*.trc\0\0")) { trcPath_ = WideToUtf8(std::wstring(buf)); recorder_.SaveToFile(Utf8ToWide(trcPath_)); SetStatusOk("已保存"); }
        }
        if (ImGui::Button("加载文件", ImVec2(-1, 0))) {
            if (recorder_.LoadFromFile(Utf8ToWide(trcPath_))) SetStatusOk("已加载"); else SetStatusError("加载失败");
        }
    }
    EndGlassCard();

    ImGui::Spacing();

    // Settings card
    BeginGlassCard("##settings_card", "回放设置", ImVec2(0, 0));
    {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("倍速");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.06f, 0.18f, 0.60f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.45f, 0.30f, 0.90f, 1.0f));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##speed", &speedFactor_, 0.1f, 10.0f, "%.1fx")) replayer_.SetSpeed(speedFactor_);
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("回放速度倍率 (0.1 - 10.0)");
        ImGui::Checkbox("屏蔽输入", &blockInput_);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("回放时屏蔽物理键鼠输入");
    }
    EndGlassCard();

    ImGui::Spacing();

    // Actions card
    BeginGlassCard("##actions_card", "操作", ImVec2(0, 0));
    {
        const float btnH = 38.0f * s;
        const bool idle = !recorder_.IsRecording() && !replayer_.IsRunning();
        if (idle) {
            if (GlowButton("开始录制  (F9)", ImVec2(-1, btnH), IM_COL32(200, 50, 80, 255), IM_COL32(220, 80, 60, 255)))
                StartRecording();
            ImGui::Spacing();
            if (GlowButton("开始回放  (F10)", ImVec2(-1, btnH), IM_COL32(40, 160, 80, 255), IM_COL32(30, 200, 120, 255)))
                StartReplay();
        } else if (recorder_.IsRecording()) {
            // Pulsing record indicator
            const float pulse = 0.7f + 0.3f * sinf(animTime_ * 4.0f);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 dotPos = ImGui::GetCursorScreenPos();
            dl->AddCircleFilled(ImVec2(dotPos.x + 8.0f * s, dotPos.y + 8.0f * s), 5.0f * s, IM_COL32(255, 60, 60, (int)(pulse * 255)));
            ImGui::Dummy(ImVec2(0, 4.0f * s));
            if (GlowButton("停止录制  (F9)", ImVec2(-1, btnH), IM_COL32(200, 140, 40, 255), IM_COL32(220, 160, 30, 255)))
                StopRecording();
        } else {
            const float progress = replayer_.Progress01();
            // Custom progress bar
            const ImVec2 barPos = ImGui::GetCursorScreenPos();
            const float barW = ImGui::GetContentRegionAvail().x;
            const float barH = 6.0f * s;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(barPos, ImVec2(barPos.x + barW, barPos.y + barH), IM_COL32(40, 30, 80, 150), 3.0f * s);
            dl->AddRectFilled(barPos, ImVec2(barPos.x + barW * progress, barPos.y + barH), IM_COL32(100, 200, 255, 220), 3.0f * s);
            // Glow at progress tip
            if (progress > 0.01f) {
                dl->AddCircleFilled(ImVec2(barPos.x + barW * progress, barPos.y + barH * 0.5f), 4.0f * s, IM_COL32(100, 200, 255, 150));
            }
            ImGui::Dummy(ImVec2(0, barH + 6.0f * s));
            if (GlowButton("停止回放  (F10)", ImVec2(-1, btnH), IM_COL32(200, 140, 40, 255), IM_COL32(220, 160, 30, 255)))
                StopReplay();
        }
    }
    EndGlassCard();

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.55f, 0.50f, 0.75f, 0.8f), "事件: %zu  |  时长: %.1f 秒",
        recorder_.Events().size(), recorder_.TotalDurationMicros() / 1'000'000.0);

    ImGui::EndChild();

    // ─── RIGHT COLUMN: Event list ───────────────────────────────────────
    ImGui::SameLine(0, gap);

    BeginGlassScrollCard("##event_list_card", "事件列表", ImVec2(rightW, availH));
    {
        if (recorder_.Events().empty()) {
            ImGui::Spacing(); ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.55f, 0.50f, 0.75f, 0.6f), "暂无事件\n\n点击「开始录制」捕获操作\n或「加载文件」打开已有录制");
        } else {
            ImGuiListClipper clipper;
            clipper.Begin((int)recorder_.Events().size());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    const auto& evt = recorder_.Events()[i];
                    ImGui::TextColored(ImVec4(0.45f, 0.40f, 0.65f, 0.8f), "%04d", i + 1);
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.82f, 0.80f, 0.92f, 1.0f), "%s", FormatEvent(evt).c_str());
                }
            }
            if (recorder_.IsRecording() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        }
    }
    EndGlassCard();
}

// ═══════════════════════════════════════════════════════════════════════════
// ADVANCED MODE
// ═══════════════════════════════════════════════════════════════════════════

void App::DrawAdvancedMode() {
    const float s = UiScale();
    const bool scriptRunning = lua_.IsRunning();

    // Toolbar
    {
        const float toolbarH = 44.0f * s;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.06f, 0.18f, 0.60f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f * s);
        ImGui::BeginChild("##toolbar", ImVec2(0, toolbarH), true, ImGuiWindowFlags_NoScrollbar);

        const float btnH = 30.0f * s;
        ImGui::SetCursorPosY((toolbarH - btnH) * 0.5f);

        if (!scriptRunning) {
            if (GlowButton(" 运行 ", ImVec2(80.0f * s, btnH), IM_COL32(40, 160, 80, 255), IM_COL32(30, 200, 120, 255))) {
                luaLastError_.clear();
                if (!lua_.StartAsync(luaEditor_)) { SetStatusError("脚本启动失败"); }
                else {
                    HWND target = RootWindowAtCursor();
                    if (target && IsWindowProcessElevated(target) && !IsCurrentProcessElevated())
                        SetStatusWarn("目标窗口是管理员权限，键盘/滚轮可能被拦截");
                    if (minimizeOnScriptRun_ && hwnd_) { ShowWindow(hwnd_, SW_MINIMIZE); scriptMinimized_ = true; }
                    luaLastHighlightLine_ = 0; SetStatusOk("脚本开始执行");
                }
            }
        } else {
            if (GlowButton(" 停止 ", ImVec2(80.0f * s, btnH), IM_COL32(200, 50, 50, 255), IM_COL32(220, 80, 60, 255))) {
                lua_.StopAsync(); SetStatusInfo("已停止脚本");
            }
        }

        ImGui::SameLine(0, 14.0f * s);
        ImGui::TextColored(ImVec4(0.45f, 0.40f, 0.65f, 0.5f), "|");
        ImGui::SameLine(0, 14.0f * s);

        ImGui::SetCursorPosY((toolbarH - btnH) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.06f, 0.18f, 0.60f));
        ImGui::SetNextItemWidth(160.0f * s);
        InputTextString("##luapath", &luaPath_);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button("打开", ImVec2(0, btnH))) {
            wchar_t buf[MAX_PATH]{}; std::wstring w = Utf8ToWide(luaPath_); wcsncpy_s(buf, w.c_str(), _TRUNCATE);
            if (OpenFileDialog(nullptr, buf, MAX_PATH, L"Lua Script (*.lua)\0*.lua\0\0")) luaPath_ = WideToUtf8(std::wstring(buf));
        }
        ImGui::SameLine();
        if (ImGui::Button("加载", ImVec2(0, btnH))) { luaEditor_ = ReadTextFile(Utf8ToWide(luaPath_)); luaLastError_.clear(); }
        ImGui::SameLine();
        if (ImGui::Button("保存", ImVec2(0, btnH))) { WriteTextFile(Utf8ToWide(luaPath_), luaEditor_); SetStatusOk("已保存"); }

        ImGui::SameLine(0, 14.0f * s);
        ImGui::TextColored(ImVec4(0.45f, 0.40f, 0.65f, 0.5f), "|");
        ImGui::SameLine(0, 14.0f * s);

        ImGui::SetCursorPosY((toolbarH - ImGui::GetFrameHeight()) * 0.5f);
        ImGui::Checkbox("最小化", &minimizeOnScriptRun_);
        ImGui::SameLine(); ImGui::Checkbox("文档", &luaUi_.docsOpen);
        ImGui::SameLine(); ImGui::Checkbox("补全", &luaUi_.assistEnabled);

        ImGui::SameLine();
        const float toolBtnW = 60.0f * s;
        const float rightPos = ImGui::GetWindowWidth() - toolBtnW - ImGui::GetStyle().WindowPadding.x;
        if (ImGui::GetCursorPosX() < rightPos) ImGui::SetCursorPosX(rightPos);
        ImGui::SetCursorPosY((toolbarH - btnH) * 0.5f);
        if (ImGui::Button("工具", ImVec2(toolBtnW, btnH))) ImGui::OpenPopup("more_tools_popup");

        ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f * s, 0), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::BeginPopup("more_tools_popup")) {
            ImGui::TextDisabled("TRC -> Lua 转换");
            ImGui::Separator();
            static float tol = 3.0f;
            ImGui::AlignTextToFramePadding();
            ImGui::Text("容差");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140.0f * s);
            ImGui::SliderFloat("##tol_slider", &tol, 0.5f, 20.0f, "%.1f px");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f * s);
            if (ImGui::InputFloat("##tol_input", &tol, 0.0f, 0.0f, "%.1f"))
                tol = std::clamp(tol, 0.5f, 20.0f);
            ImGui::Checkbox("高保真导出", &exportFull_);
            if (ImGui::Button("执行转换", ImVec2(-1, 0))) {
                bool ok = exportFull_ ? Converter::TrcToLuaFull(Utf8ToWide(trcPath_), Utf8ToWide(luaPath_)) : Converter::TrcToLua(Utf8ToWide(trcPath_), Utf8ToWide(luaPath_), tol);
                if (ok) { luaEditor_ = ReadTextFile(Utf8ToWide(luaPath_)); luaLastError_.clear(); SetStatusOk("导出成功"); ImGui::CloseCurrentPopup(); }
                else SetStatusError("导出失败");
            }
            ImGui::EndPopup();
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    // Error display
    if (!luaLastError_.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.30f, 0.08f, 0.10f, 0.70f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f * s);
        const float errH = ImGui::CalcTextSize(luaLastError_.c_str(), nullptr, false, ImGui::GetContentRegionAvail().x - 20.0f * s).y + 16.0f * s;
        ImGui::BeginChild("##error_bar", ImVec2(0, errH), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "错误: %s", luaLastError_.c_str());
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    // Execution status
    if (scriptRunning) {
        const int curLine = lua_.CurrentLine();
        ImGui::Spacing();
        if (curLine > 0) ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "正在执行  行 %d", curLine);
        else ImGui::TextColored(ImVec4(0.6f, 0.55f, 0.8f, 0.8f), "正在执行...");
    }

    // Editor + Docs
    ImGui::Spacing();
    const int curLine = scriptRunning ? lua_.CurrentLine() : 0;
    const float editorH = ImGui::GetContentRegionAvail().y - 40.0f * s;

    // Record editor screen rect for WndProc scroll filtering
    {
        const ImVec2 cp = ImGui::GetCursorScreenPos();
        const float ew = ImGui::GetContentRegionAvail().x;
        editorScreenRect_ = { (LONG)cp.x, (LONG)cp.y, (LONG)(cp.x + ew), (LONG)(cp.y + editorH) };
        editorRectValid_ = true;
    }

    if (luaUi_.docsOpen) {
        if (ImGui::BeginTable("##lua_layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0, editorH))) {
            ImGui::TableSetupColumn("编辑器", ImGuiTableColumnFlags_WidthStretch, 0.72f);
            ImGui::TableSetupColumn("文档", ImGuiTableColumnFlags_WidthStretch, 0.28f);
            ImGui::TableNextColumn();
            DrawLuaEditorWithLineNumbers(&luaUi_, &luaEditor_, -1.0f, scriptRunning, curLine, &luaLastHighlightLine_);
            ImGui::TableNextColumn();
            DrawLuaDocsPanel(&luaUi_, -1.0f, false);
            ImGui::EndTable();
        }
    } else {
        DrawLuaEditorWithLineNumbers(&luaUi_, &luaEditor_, editorH, scriptRunning, curLine, &luaLastHighlightLine_);
    }
}

// ─── Status bar ─────────────────────────────────────────────────────────────

void App::DrawStatusBar() {
    const int64_t now = timing::MicrosNow();
    if (statusText_.empty() || now >= statusExpireMicros_) return;
    const float s = UiScale();

    ImU32 textCol = IM_COL32(180, 170, 210, 200);
    ImU32 bgCol = IM_COL32(30, 25, 60, 180);
    ImU32 accentCol = IM_COL32(100, 80, 200, 150);
    const char* icon = "";

    if (statusLevel_ == 1) { textCol = IM_COL32(80, 230, 130, 240); bgCol = IM_COL32(20, 50, 35, 200); accentCol = IM_COL32(60, 200, 100, 200); icon = "OK  "; }
    if (statusLevel_ == 2) { textCol = IM_COL32(255, 200, 80, 240); bgCol = IM_COL32(50, 40, 20, 200); accentCol = IM_COL32(220, 180, 50, 200); icon = "!!  "; }
    if (statusLevel_ == 3) { textCol = IM_COL32(255, 100, 100, 240); bgCol = IM_COL32(50, 20, 20, 200); accentCol = IM_COL32(220, 60, 60, 200); icon = "ERR  "; }

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f * s);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##status_bar", ImVec2(0, 30.0f * s), false, ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), bgCol, 6.0f * s);
    // Left accent bar
    dl->AddRectFilled(wp, ImVec2(wp.x + 3.0f * s, wp.y + ws.y), accentCol, 6.0f * s);

    ImGui::SetCursorPosY((ws.y - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::SetCursorPosX(12.0f * s);

    char fullText[512];
    snprintf(fullText, sizeof(fullText), "%s%s", icon, statusText_.c_str());
    dl->AddText(ImVec2(wp.x + 12.0f * s, wp.y + (ws.y - ImGui::GetTextLineHeight()) * 0.5f), textCol, fullText);
    ImGui::Dummy(ImVec2(0, 0)); // keep child alive

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ─── Block input confirmation modal ─────────────────────────────────────────

void App::DrawBlockInputConfirmModal() {
    if (blockInputConfirmOpen_) { blockInputUnderstood_ = false; ImGui::OpenPopup("确认屏蔽系统输入"); blockInputConfirmOpen_ = false; }
    bool open = true;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.10f, 0.25f, 0.95f));
    if (ImGui::BeginPopupModal("确认屏蔽系统输入", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        const float s = UiScale();
        ImGui::TextUnformatted("你即将启用屏蔽系统输入（BlockInput）。");
        ImGui::TextUnformatted("启用后鼠标键盘可能暂时不可用。");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "紧急停止：Ctrl+F12");
        ImGui::Separator();
        ImGui::Checkbox("我已理解风险", &blockInputUnderstood_);
        ImGui::Separator();
        const bool canContinue = blockInputUnderstood_;
        if (!canContinue) ImGui::BeginDisabled();
        if (GlowButton("继续启用并回放", ImVec2(180.0f * s, 32.0f * s), IM_COL32(80, 60, 200, 255), IM_COL32(120, 60, 220, 255))) {
            pendingStartReplay_ = false; ImGui::CloseCurrentPopup(); StartReplayConfirmed();
        }
        if (!canContinue) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(120.0f * s, 32.0f * s))) { pendingStartReplay_ = false; ImGui::CloseCurrentPopup(); SetStatusInfo("已取消回放"); }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

// ─── Status setters ─────────────────────────────────────────────────────────

void App::RequestExit() { exitConfirmOpen_ = true; }
bool App::ShouldExit() const { return exitConfirmed_; }

void App::DrawExitConfirmModal() {
    if (exitConfirmOpen_) { ImGui::OpenPopup("退出确认"); exitConfirmOpen_ = false; }
    bool open = true;
    // Center the modal on the viewport every frame so it stays centered in fullscreen
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.10f, 0.25f, 0.95f));
    if (ImGui::BeginPopupModal("退出确认", &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        const float s = UiScale();
        ImGui::Spacing();
        ImGui::TextUnformatted("确定要退出 AutoClicker-Pro 吗？");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (GlowButton("确定", ImVec2(140.0f * s, 32.0f * s), IM_COL32(180, 60, 80, 255), IM_COL32(220, 80, 100, 255))) {
            exitConfirmed_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(120.0f * s, 32.0f * s))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

void App::SetStatusInfo(const std::string& text)  { statusLevel_ = 0; statusText_ = text; statusExpireMicros_ = timing::MicrosNow() + 3'000'000; }
void App::SetStatusOk(const std::string& text)    { statusLevel_ = 1; statusText_ = text; statusExpireMicros_ = timing::MicrosNow() + 3'000'000; }
void App::SetStatusWarn(const std::string& text)   { statusLevel_ = 2; statusText_ = text; statusExpireMicros_ = timing::MicrosNow() + 6'000'000; }
void App::SetStatusError(const std::string& text)  { statusLevel_ = 3; statusText_ = text; statusExpireMicros_ = timing::MicrosNow() + 8'000'000; }

// ─── Recording / Replay ─────────────────────────────────────────────────────

void App::StartRecording() { EmergencyStop(); recorder_.Start(); hooks_.Install(&recorder_); recordStartQpc_ = timing::QpcNow(); overlay_.SetRecording(true); overlay_.SetElapsedMicros(0); overlay_.Show(); }
void App::StopRecording() { hooks_.Uninstall(); recorder_.Stop(); overlay_.SetRecording(false); overlay_.Hide(); }
void App::StartReplay() { if (blockInput_) { pendingStartReplay_ = true; blockInputConfirmOpen_ = true; return; } StartReplayConfirmed(); }
void App::StartReplayConfirmed() {
    if (recorder_.IsRecording()) StopRecording();
    if (!recorder_.LoadFromFile(Utf8ToWide(trcPath_))) { SetStatusError("回放失败：无法读取 .trc"); return; }
    const auto& ev = recorder_.Events(); std::vector<trc::RawEvent> copy(ev.begin(), ev.end());
    replayer_.SetSpeed(speedFactor_);
    if (replayer_.Start(std::move(copy), blockInput_, speedFactor_)) SetStatusOk("已开始回放"); else SetStatusError("回放失败");
}
void App::StopReplay() { replayer_.Stop(); SetStatusInfo("已停止回放"); }
void App::EmergencyStop() { lua_.StopAsync(); hooks_.Uninstall(); recorder_.Stop(); replayer_.Stop(); overlay_.SetRecording(false); overlay_.Hide(); SetStatusOk("已紧急停止"); }

// ─── File dialogs ───────────────────────────────────────────────────────────

bool App::OpenFileDialog(HWND owner, wchar_t* path, uint32_t pathCapacity, const wchar_t* filter) {
    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner; ofn.lpstrFile = path; ofn.nMaxFile = pathCapacity; ofn.lpstrFilter = filter; ofn.nFilterIndex = 1; ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER; return GetOpenFileNameW(&ofn) != FALSE;
}
bool App::SaveFileDialog(HWND owner, wchar_t* path, uint32_t pathCapacity, const wchar_t* filter) {
    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner; ofn.lpstrFile = path; ofn.nMaxFile = pathCapacity; ofn.lpstrFilter = filter; ofn.nFilterIndex = 1; ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER; return GetSaveFileNameW(&ofn) != FALSE;
}
std::string App::ReadTextFile(const std::wstring& filename) { std::ifstream in(std::filesystem::path(filename), std::ios::binary); if (!in) return {}; std::ostringstream ss; ss << in.rdbuf(); return ss.str(); }
bool App::WriteTextFile(const std::wstring& filename, const std::string& content) { std::ofstream out(std::filesystem::path(filename), std::ios::binary); if (!out) return false; out.write(content.data(), (std::streamsize)content.size()); return out.good(); }

// ─── Config save/load ───────────────────────────────────────────────────────

void App::LoadConfig() {
    const std::wstring configPath = L"config.ini";
    std::ifstream in(configPath);
    if (!in) return;

    std::string line;
    while (std::getline(in, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        
        // Parse key=value
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        
        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
        };
        trim(key);
        trim(value);
        
        // Apply settings
        if (key == "mode") mode_ = std::atoi(value.c_str());
        else if (key == "blockInput") blockInput_ = (value == "1" || value == "true");
        else if (key == "speedFactor") speedFactor_ = (float)std::atof(value.c_str());
        else if (key == "trcPath") trcPath_ = value;
        else if (key == "luaPath") luaPath_ = value;
        else if (key == "exportFull") exportFull_ = (value == "1" || value == "true");
        else if (key == "minimizeOnScriptRun") minimizeOnScriptRun_ = (value == "1" || value == "true");
        else if (key == "docsOpen") luaUi_.docsOpen = (value == "1" || value == "true");
        else if (key == "assistEnabled") luaUi_.assistEnabled = (value == "1" || value == "true");
    }
}

void App::SaveConfig() {
    const std::wstring configPath = L"config.ini";
    std::ofstream out(configPath);
    if (!out) return;
    
    out << "# AutoClicker-Pro Configuration\n";
    out << "# This file is automatically generated\n\n";
    
    out << "# UI Mode (0=录制回放, 1=Lua脚本)\n";
    out << "mode=" << mode_ << "\n\n";
    
    out << "# Playback Settings\n";
    out << "blockInput=" << (blockInput_ ? "1" : "0") << "\n";
    out << "speedFactor=" << speedFactor_ << "\n\n";
    
    out << "# File Paths\n";
    out << "trcPath=" << trcPath_ << "\n";
    out << "luaPath=" << luaPath_ << "\n\n";
    
    out << "# Export Settings\n";
    out << "exportFull=" << (exportFull_ ? "1" : "0") << "\n\n";
    
    out << "# Script Settings\n";
    out << "minimizeOnScriptRun=" << (minimizeOnScriptRun_ ? "1" : "0") << "\n";
    out << "docsOpen=" << (luaUi_.docsOpen ? "1" : "0") << "\n";
    out << "assistEnabled=" << (luaUi_.assistEnabled ? "1" : "0") << "\n";
}
