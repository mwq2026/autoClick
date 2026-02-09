#include <windows.h>
#include <d3d11.h>
#include <tchar.h>

#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "app/App.h"
#include "resources/resource.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static constexpr float kBaseUiFontPx = 18.0f;
static float g_uiDpiScale = 1.0f;
static ImGuiStyle g_baseStyle{};
static bool g_imguiBackendReady = false;

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        &featureLevel,
        &g_pd3dDeviceContext);
    if (res != S_OK) return false;
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static App* g_app = nullptr;

static void EnableBestEffortDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
        auto* setCtx = reinterpret_cast<SetProcessDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setCtx) {
            if (setCtx(reinterpret_cast<HANDLE>(-4))) return;
        }
    }

    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);
        auto* setAwareness = reinterpret_cast<SetProcessDpiAwarenessFn>(GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (setAwareness) {
            const HRESULT hr = setAwareness(2);
            FreeLibrary(shcore);
            if (SUCCEEDED(hr)) return;
        }
        FreeLibrary(shcore);
    }

    if (user32) {
        using SetProcessDPIAwareFn = BOOL(WINAPI*)();
        auto* setAware = reinterpret_cast<SetProcessDPIAwareFn>(GetProcAddress(user32, "SetProcessDPIAware"));
        if (setAware) (void)setAware();
    }
}

static bool FileExists(const std::wstring& path) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}

static std::wstring GetWindowsFontsDir() {
    wchar_t winDir[MAX_PATH]{};
    UINT n = GetWindowsDirectoryW(winDir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return L"C:\\Windows\\Fonts";
    return JoinPath(winDir, L"Fonts");
}

static bool SetupChineseFonts(ImGuiIO& io, float baseFontSizePx) {
    const std::wstring fontsDir = GetWindowsFontsDir();
    const std::vector<std::wstring> candidates = {
        JoinPath(fontsDir, L"msyh.ttc"),
        JoinPath(fontsDir, L"msyh.ttf"),
        JoinPath(fontsDir, L"msyhl.ttc"),
        JoinPath(fontsDir, L"simhei.ttf"),
        JoinPath(fontsDir, L"simsun.ttc"),
        JoinPath(fontsDir, L"arialuni.ttf")
    };

    std::wstring chosen;
    for (const auto& p : candidates) {
        if (FileExists(p)) {
            chosen = p;
            break;
        }
    }
    if (chosen.empty()) return false;

    auto wideToUtf8 = [](const std::wstring& s) -> std::string {
        if (s.empty()) return {};
        const int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
        std::string out;
        out.resize(static_cast<size_t>(len));
        WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
        return out;
    };

    ImFontConfig cfg{};
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;

    const ImWchar* ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    const std::string pathUtf8 = wideToUtf8(chosen);
    io.FontDefault = io.Fonts->AddFontFromFileTTF(
        pathUtf8.c_str(),
        baseFontSizePx,
        &cfg,
        ranges);
    return io.FontDefault != nullptr;
}

static UINT QueryDpiForSystem() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using GetDpiForSystemFn = UINT(WINAPI*)();
        auto* getDpiForSystem = reinterpret_cast<GetDpiForSystemFn>(GetProcAddress(user32, "GetDpiForSystem"));
        if (getDpiForSystem) return getDpiForSystem();
    }

    HDC dc = GetDC(nullptr);
    UINT dpi = 96;
    if (dc) {
        dpi = static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX));
        ReleaseDC(nullptr, dc);
    }
    return dpi ? dpi : 96;
}

static UINT QueryDpiForWindow(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        auto* getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
        if (getDpiForWindow) return getDpiForWindow(hwnd);
    }

    HDC dc = GetDC(hwnd);
    UINT dpi = 96;
    if (dc) {
        dpi = static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX));
        ReleaseDC(hwnd, dc);
    }
    return dpi ? dpi : 96;
}

static float DpiScaleFromDpi(UINT dpi) {
    return static_cast<float>(dpi) / 96.0f;
}

static void ApplyDpiToImGui(float dpiScale) {
    if (!ImGui::GetCurrentContext()) {
        g_uiDpiScale = dpiScale;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontDefault = nullptr;
    io.FontGlobalScale = 1.0f;

    const bool fontOk = SetupChineseFonts(io, kBaseUiFontPx * dpiScale);
    if (!fontOk) {
        io.Fonts->AddFontDefault();
        io.FontGlobalScale = dpiScale;
    }

    ImGuiStyle style = g_baseStyle;
    style.ScaleAllSizes(dpiScale);
    ImGui::GetStyle() = style;

    if (g_imguiBackendReady) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        ImGui_ImplDX11_CreateDeviceObjects();
    }

    g_uiDpiScale = dpiScale;
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DPICHANGED) {
        const UINT dpi = LOWORD(wParam);
        const float scale = DpiScaleFromDpi(dpi ? dpi : 96);
        auto* suggested = reinterpret_cast<RECT*>(lParam);
        if (suggested) {
            SetWindowPos(
                hWnd,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        ApplyDpiToImGui(scale);
        return 0;
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_HOTKEY:
        if (g_app) g_app->OnHotkey();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    EnableBestEffortDpiAwareness();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"AutoClickerPro";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    const float initialScale = DpiScaleFromDpi(QueryDpiForSystem());
    const int initialW = static_cast<int>(980.0f * initialScale);
    const int initialH = static_cast<int>(640.0f * initialScale);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"AutoClicker-Pro", WS_OVERLAPPEDWINDOW, 100, 100, initialW, initialH, nullptr, nullptr, wc.hInstance, nullptr);
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    RegisterHotKey(hwnd, 1, MOD_CONTROL, VK_F12);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    g_baseStyle = ImGui::GetStyle();
    g_uiDpiScale = DpiScaleFromDpi(QueryDpiForWindow(hwnd));
    ApplyDpiToImGui(g_uiDpiScale);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    g_imguiBackendReady = true;

    App app(hInstance, hwnd);
    g_app = &app;

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        app.OnFrame();

        ImGui::Render();
        const float clear_color[4] = { 0.08f, 0.09f, 0.10f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    g_app = nullptr;
    UnregisterHotKey(hwnd, 1);

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
