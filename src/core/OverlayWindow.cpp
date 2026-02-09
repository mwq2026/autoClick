#include "core/OverlayWindow.h"

#include <algorithm>

static constexpr COLORREF kKeyColor = RGB(255, 0, 255);

OverlayWindow::OverlayWindow() = default;

OverlayWindow::~OverlayWindow() {
    Destroy();
}

bool OverlayWindow::Create(HINSTANCE hInstance) {
    if (hwnd_) return true;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &OverlayWindow::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"AutoClickerProOverlay";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"",
        WS_POPUP,
        10, 10, 260, 32,
        nullptr, nullptr, hInstance, this);

    if (!hwnd_) return false;

    SetLayeredWindowAttributes(hwnd_, kKeyColor, 0, LWA_COLORKEY);
    return true;
}

void OverlayWindow::Destroy() {
    if (!hwnd_) return;
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
}

void OverlayWindow::Show() {
    if (!hwnd_) return;
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd_);
}

void OverlayWindow::Hide() {
    if (!hwnd_) return;
    ShowWindow(hwnd_, SW_HIDE);
}

bool OverlayWindow::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_) != FALSE;
}

void OverlayWindow::SetRecording(bool recording) {
    recording_ = recording;
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

void OverlayWindow::SetElapsedMicros(int64_t elapsedMicros) {
    elapsedMicros_ = std::max<int64_t>(0, elapsedMicros);
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<OverlayWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT OverlayWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd_, &ps);
        Render(hdc);
        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void OverlayWindow::Render(HDC hdc) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    HBRUSH bg = CreateSolidBrush(kKeyColor);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    const int pad = 6;
    const int dot = 10;
    const int cy = (rc.bottom - rc.top) / 2;
    const int dotX = pad;
    const int dotY = cy - dot / 2;

    HBRUSH red = CreateSolidBrush(RGB(230, 40, 40));
    HGDIOBJ oldBrush = SelectObject(hdc, red);
    Ellipse(hdc, dotX, dotY, dotX + dot, dotY + dot);
    SelectObject(hdc, oldBrush);
    DeleteObject(red);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(245, 245, 245));

    const double seconds = static_cast<double>(elapsedMicros_) / 1'000'000.0;
    wchar_t buf[128]{};
    if (recording_) {
        swprintf_s(buf, L"Recording...  %.3fs", seconds);
    } else {
        swprintf_s(buf, L"");
    }

    RECT textRc = rc;
    textRc.left = dotX + dot + pad;
    DrawTextW(hdc, buf, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}
