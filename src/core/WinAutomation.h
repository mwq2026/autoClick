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

// ─── Spy++ / UI Automation extensions ───────────────────────────────────────

// Window hierarchy
HWND WindowParent(HWND hwnd);
HWND WindowOwner(HWND hwnd);
HWND WindowChild(HWND hwnd);           // first child
HWND WindowNextSibling(HWND hwnd);
HWND WindowPrevSibling(HWND hwnd);
std::vector<HWND> WindowChildren(HWND hwnd, bool recursive);
HWND WindowDesktop();

// Window properties
uint32_t WindowStyle(HWND hwnd);
uint32_t WindowExStyle(HWND hwnd);
bool WindowSetStyle(HWND hwnd, uint32_t style);
bool WindowSetExStyle(HWND hwnd, uint32_t exStyle);
bool WindowIsVisible(HWND hwnd);
bool WindowIsEnabled(HWND hwnd);
bool WindowIsFocused(HWND hwnd);
bool WindowIsMinimized(HWND hwnd);
bool WindowIsMaximized(HWND hwnd);
uint32_t WindowThreadId(HWND hwnd);
int WindowTextLength(HWND hwnd);

// Control text (SendMessage WM_GETTEXT / WM_SETTEXT)
std::wstring ControlGetText(HWND hwnd);
bool ControlSetText(HWND hwnd, const std::wstring& text);

// Window enable/disable/focus
bool WindowEnable(HWND hwnd, bool enable);
bool WindowSetFocus(HWND hwnd);

// SendMessage / PostMessage
intptr_t WindowSendMessage(HWND hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
bool WindowPostMessage(HWND hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);

// Control-specific helpers (via SendMessage)
bool ButtonClick(HWND hwnd);                           // BM_CLICK
int  CheckboxGetState(HWND hwnd);                      // BM_GETCHECK
bool CheckboxSetState(HWND hwnd, int state);            // BM_SETCHECK
int  ComboboxGetCurSel(HWND hwnd);                     // CB_GETCURSEL
bool ComboboxSetCurSel(HWND hwnd, int index);           // CB_SETCURSEL
int  ComboboxGetCount(HWND hwnd);                       // CB_GETCOUNT
std::wstring ComboboxGetItem(HWND hwnd, int index);     // CB_GETLBTEXT
int  ListboxGetCurSel(HWND hwnd);                       // LB_GETCURSEL
bool ListboxSetCurSel(HWND hwnd, int index);             // LB_SETCURSEL
int  ListboxGetCount(HWND hwnd);                         // LB_GETCOUNT
std::wstring ListboxGetItem(HWND hwnd, int index);       // LB_GETTEXT
int  EditGetLineCount(HWND hwnd);                        // EM_GETLINECOUNT
std::wstring EditGetLine(HWND hwnd, int line);           // EM_GETLINE
bool EditSetSel(HWND hwnd, int start, int end);          // EM_SETSEL
bool EditReplaceSel(HWND hwnd, const std::wstring& text);// EM_REPLACESEL
int  EditGetSel(HWND hwnd, int* startOut, int* endOut);  // EM_GETSEL

// Scroll
bool ScrollWindow(HWND hwnd, int bar, int pos);         // SBM_SETPOS / WM_VSCROLL/WM_HSCROLL
int  ScrollGetPos(HWND hwnd, int bar);
int  ScrollGetRange(HWND hwnd, int bar, int* minOut, int* maxOut);

// Tab control
int  TabGetCurSel(HWND hwnd);
bool TabSetCurSel(HWND hwnd, int index);
int  TabGetCount(HWND hwnd);

// TreeView basic
int  TreeViewGetCount(HWND hwnd);
intptr_t TreeViewGetSelection(HWND hwnd);
bool TreeViewSelectItem(HWND hwnd, intptr_t hItem);

// ListView basic
int  ListViewGetItemCount(HWND hwnd);
int  ListViewGetSelectedCount(HWND hwnd);
int  ListViewGetNextSelected(HWND hwnd, int start);

// Window find by child path (class chain)
HWND FindChildByClass(HWND parent, const std::wstring& className, int index);
HWND FindChildByText(HWND parent, const std::wstring& textSubstr);

// Screen capture region to file
bool ScreenCaptureRect(int x, int y, int w, int h, const std::wstring& bmpPath);

// System info
int  GetMonitorCount();
bool GetMonitorRect(int index, RECT* out);
uint32_t GetSystemDpi();
uint32_t GetWindowDpi(HWND hwnd);

// Registry
std::wstring RegReadString(const std::wstring& key, const std::wstring& valueName);
bool RegWriteString(const std::wstring& key, const std::wstring& valueName, const std::wstring& data);
uint32_t RegReadDword(const std::wstring& key, const std::wstring& valueName, uint32_t defaultVal);
bool RegWriteDword(const std::wstring& key, const std::wstring& valueName, uint32_t data);

// Environment
std::wstring EnvGet(const std::wstring& name);
bool EnvSet(const std::wstring& name, const std::wstring& value);

// File system helpers
bool FileExists(const std::wstring& path);
bool DirExists(const std::wstring& path);
bool FileDelete(const std::wstring& path);
bool DirCreate(const std::wstring& path);
uint64_t FileSize(const std::wstring& path);

// Message box (for debugging scripts)
int MsgBox(const std::wstring& text, const std::wstring& title, uint32_t flags);

} // namespace winauto
