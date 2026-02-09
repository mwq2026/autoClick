#include "core/Hooks.h"

#include <windows.h>

#include "core/HighResClock.h"
#include "core/Recorder.h"
#include "core/TrcFormat.h"

static Hooks* g_hooks = nullptr;

Hooks::Hooks() = default;

Hooks::~Hooks() {
    Uninstall();
}

bool Hooks::Install(Recorder* recorder) {
    if (IsInstalled()) return true;
    if (g_hooks != nullptr) return false;
    if (recorder == nullptr) return false;

    recorder_ = recorder;
    lastQpc_.store(0, std::memory_order_release);
    g_hooks = this;

    HINSTANCE hinst = GetModuleHandleW(nullptr);
    mouse_ = SetWindowsHookExW(WH_MOUSE_LL, &Hooks::MouseProc, hinst, 0);
    key_ = SetWindowsHookExW(WH_KEYBOARD_LL, &Hooks::KeyProc, hinst, 0);

    if (!mouse_ || !key_) {
        Uninstall();
        return false;
    }
    return true;
}

void Hooks::Uninstall() {
    if (mouse_) {
        UnhookWindowsHookEx(mouse_);
        mouse_ = nullptr;
    }
    if (key_) {
        UnhookWindowsHookEx(key_);
        key_ = nullptr;
    }
    recorder_ = nullptr;
    lastQpc_.store(0, std::memory_order_release);
    if (g_hooks == this) g_hooks = nullptr;
}

bool Hooks::IsInstalled() const {
    return mouse_ != nullptr || key_ != nullptr;
}

LRESULT CALLBACK Hooks::MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && g_hooks && g_hooks->recorder_) {
        const auto* ms = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        if (ms) g_hooks->OnMouse(wParam, *ms);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK Hooks::KeyProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && g_hooks && g_hooks->recorder_) {
        const auto* ks = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (ks) g_hooks->OnKey(wParam, *ks);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

int64_t Hooks::NextDeltaMicros() {
    const int64_t now = timing::QpcNow();
    const int64_t prev = lastQpc_.exchange(now, std::memory_order_acq_rel);
    if (prev == 0) return 0;
    return timing::QpcDeltaToMicros(now - prev);
}

static int ButtonFromWParam(WPARAM wParam, const MSLLHOOKSTRUCT& ms) {
    switch (wParam) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        return 1;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        return 2;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
        return 3;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
        return (HIWORD(ms.mouseData) == XBUTTON1) ? 4 : 5;
    default:
        return 0;
    }
}

void Hooks::OnMouse(WPARAM wParam, const MSLLHOOKSTRUCT& ms) {
    trc::RawEvent e{};
    e.x = static_cast<int32_t>(ms.pt.x);
    e.y = static_cast<int32_t>(ms.pt.y);
    e.timeDelta = NextDeltaMicros();

    if (wParam == WM_MOUSEMOVE) {
        e.type = static_cast<uint8_t>(trc::EventType::MouseMove);
        recorder_->PushRawEvent(e);
        return;
    }

    if (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) {
        e.type = static_cast<uint8_t>(trc::EventType::Wheel);
        const int16_t delta = static_cast<int16_t>(HIWORD(ms.mouseData));
        e.data = static_cast<int32_t>(static_cast<uint16_t>(delta));
        if (wParam == WM_MOUSEHWHEEL) e.data |= (1 << 30);
        recorder_->PushRawEvent(e);
        return;
    }

    if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN) {
        e.type = static_cast<uint8_t>(trc::EventType::MouseDown);
        e.data = ButtonFromWParam(wParam, ms);
        recorder_->PushRawEvent(e);
        return;
    }

    if (wParam == WM_LBUTTONUP || wParam == WM_RBUTTONUP || wParam == WM_MBUTTONUP || wParam == WM_XBUTTONUP) {
        e.type = static_cast<uint8_t>(trc::EventType::MouseUp);
        e.data = ButtonFromWParam(wParam, ms);
        recorder_->PushRawEvent(e);
        return;
    }
}

void Hooks::OnKey(WPARAM wParam, const KBDLLHOOKSTRUCT& ks) {
    trc::RawEvent e{};
    e.x = static_cast<int32_t>(ks.vkCode);
    e.y = static_cast<int32_t>(ks.scanCode);
    e.data = static_cast<int32_t>(ks.flags);
    e.timeDelta = NextDeltaMicros();

    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        e.type = static_cast<uint8_t>(trc::EventType::KeyDown);
        recorder_->PushRawEvent(e);
        return;
    }

    if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
        e.type = static_cast<uint8_t>(trc::EventType::KeyUp);
        recorder_->PushRawEvent(e);
        return;
    }
}
