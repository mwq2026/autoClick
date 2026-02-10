#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

namespace winauto {

HWND RootWindowAtSkipSelf(const POINT& pt);
HWND WindowFromPointSkipSelf(const POINT& pt);

bool ActivateWindow(HWND hwnd);

std::wstring WindowTitle(HWND hwnd);
std::wstring WindowClass(HWND hwnd);
uint32_t WindowPid(HWND hwnd);

bool WindowRect(HWND hwnd, RECT* out);
bool WindowClientSize(HWND hwnd, int* wOut, int* hOut);

bool WindowSetTopmost(HWND hwnd, bool on);
bool WindowBringToTop(HWND hwnd);
bool WindowSendToBack(HWND hwnd);

bool WindowShow(HWND hwnd);
bool WindowHide(HWND hwnd);
bool WindowMinimize(HWND hwnd);
bool WindowMaximize(HWND hwnd);
bool WindowRestore(HWND hwnd);

bool WindowMove(HWND hwnd, int x, int y);
bool WindowResize(HWND hwnd, int w, int h);
bool WindowSetRect(HWND hwnd, int x, int y, int w, int h);

bool WindowClose(HWND hwnd);
bool WindowCloseForce(HWND hwnd, uint32_t waitMs);

std::vector<HWND> FindWindowsByTitleContains(const std::wstring& titleSubstr, const std::wstring& className, bool visibleOnly, bool skipSelf);

uint32_t ProcessStart(const std::wstring& path, const std::wstring& args, const std::wstring& cwd);
bool ProcessIsRunning(uint32_t pid);
bool ProcessWait(uint32_t pid, uint32_t timeoutMs);
bool ProcessKill(uint32_t pid, uint32_t exitCode);

bool ClipboardSetText(const std::wstring& text);
std::wstring ClipboardGetText();

bool CursorPos(POINT* out);
bool CursorSet(int x, int y);
bool ScreenSize(int* wOut, int* hOut);

bool PixelGet(int x, int y, uint8_t* rOut, uint8_t* gOut, uint8_t* bOut);

} // namespace winauto
