#pragma once

#include <cstdint>
#include <string>
#include <windows.h>

class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    bool Create(HINSTANCE hInstance);
    void Destroy();

    void Show();
    void Hide();
    bool IsVisible() const;

    void SetRecording(bool recording);
    void SetElapsedMicros(int64_t elapsedMicros);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void Render(HDC hdc);

    HWND hwnd_{ nullptr };
    bool recording_{ false };
    int64_t elapsedMicros_{ 0 };
};
