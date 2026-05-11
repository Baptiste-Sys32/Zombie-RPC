#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include "dependencies/imgui/imgui.h"
#include "dependencies/imgui/imgui_impl_win32.h"
#include "dependencies/imgui/imgui_impl_dx11.h"
#include "Global.h"
#include "Interface.h"
#include "DiscordManager.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "resource.h"

// Linker Dependencies
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ws2_32.lib") 

// Global Instances
PresenceSettings g_Settings;
bool g_ExitRequested = false;
NOTIFYICONDATAW nid = {};

// Direct3D 11 Global Objects
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

/* Forward Declarations */
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* Extracts the Discord-RPC DLL from binary resources to the AppData directory */
bool EnsureDiscordDLL(const std::string& targetPath) {
    if (GetFileAttributesA(targetPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }

    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_DISCORD_DLL), RT_RCDATA);
    if (!hRes) return false;

    HGLOBAL hData = LoadResource(NULL, hRes);
    void* pData = LockResource(hData);
    DWORD size = SizeofResource(NULL, hRes);

    std::ofstream ofs(targetPath, std::ios::binary);
    if (ofs.is_open()) {
        ofs.write(reinterpret_cast<const char*>(pData), size);
        return true;
    }

    return false;
}

/* Main Application Entry Point */
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    /* --- Single Instance Check --- */
    // Create a unique mutex name for ZombieRPC
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"ZombieRPC_SingleInstanceMutex");

    if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is running. Try to find its window and bring it to the front.
        HWND existingHwnd = FindWindowW(L"ZombieRPC_Class", NULL);
        if (existingHwnd) {
            ShowWindow(existingHwnd, SW_SHOW);
            SetForegroundWindow(existingHwnd);
        }

        if (hMutex) CloseHandle(hMutex);
        return 0; // Exit this instance
    }

    // 1. Prepare DLL Redirection from AppData
    std::string dllPath = (g_Config.GetAppDataPath() / "discord-rpc.dll").string();

    if (!EnsureDiscordDLL(dllPath)) {
        MessageBoxA(NULL, "Failed to initialize system dependencies in AppData.", "ZombieRPC Error", MB_ICONERROR);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // 2. Set DLL Search Directory to allow the delay-loaded DLL to be found
    SetDllDirectoryW(g_Config.GetAppDataPath().wstring().c_str());

    // 3. Initialize Window Class
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ZombieRPC_Class", nullptr };
    wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    ::RegisterClassExW(&wc);

    // 4. Handle Startup Flags (Minimized mode)
    LPWSTR cmd = GetCommandLineW();
    bool startMinimized = (wcsstr(cmd, L"-minimized") != nullptr);

    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"ZombieRPC", WS_POPUP, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // 5. System Tray (Shell_NotifyIcon) Setup
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    lstrcpyW(nid.szTip, L"ZombieRPC - Control Panel");
    Shell_NotifyIconW(NIM_ADD, &nid);

    // Show window only if not started with -minimized
    if (!startMinimized) {
        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(hwnd);
    }

    // 6. ImGui Initialization
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& imguiIo = ImGui::GetIO();
    imguiIo.IniFilename = nullptr; // Prevent creation of imgui.ini
    imguiIo.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    g_Logger.AddLog("[Zombie-RPC] Discord-RPC Engine started.", ImVec4(0.5f, 0.5f, 1.0f, 1.0f));

    // 7. Auto-load default profile if set
    std::string defaultPreset = g_Config.GetDefaultPresetName();
    if (!defaultPreset.empty()) {
        if (g_Config.LoadPreset(defaultPreset, g_Settings)) {
            g_Logger.AddLog("[Zombie-RPC] Automatically loaded preset: " + defaultPreset, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
            if (strlen(g_Settings.clientId) > 10) {
                g_Discord.Initialize(g_Settings.clientId);
            }
        }
    }

    // 8. Main Message Loop
    while (!g_ExitRequested) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_ExitRequested = true;
        }
        if (g_ExitRequested) break;

        // Resource optimization: Sleep if window is hidden (minimized to tray)
        if (!IsWindowVisible(hwnd)) {
            Sleep(10);
            continue;
        }

        // Start Frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Interface::Render(g_ExitRequested, hwnd);

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // V-Sync enabled
    }

    // 9. Shutdown and Cleanup
    Shell_NotifyIconW(NIM_DELETE, &nid);
    g_Discord.Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    // Release Mutex
    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return 0;
}

/* Win32 Message Handler */
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            ShowWindow(hWnd, SW_SHOW);
            SetForegroundWindow(hWnd);
        }
        else if (LOWORD(lParam) == WM_RBUTTONUP) {
            HMENU hMenu = CreatePopupMenu();
            HMENU hProfileMenu = CreatePopupMenu();

            // Populate quick-swap menu from available presets
            auto presets = g_Config.GetAvailablePresets();
            for (size_t i = 0; i < presets.size(); i++) {
                AppendMenuA(hProfileMenu, MF_STRING, ID_TRAY_PROFILES_START + (UINT_PTR)i, presets[i].c_str());
            }

            AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Show ZombieRPC");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hProfileMenu, L"Quick-Swap Profile");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Quit");

            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
        }
        return 0;

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);

        // Handle Dynamic Profile Swapping from Tray
        if (wmId >= ID_TRAY_PROFILES_START) {
            auto presets = g_Config.GetAvailablePresets();
            size_t index = (size_t)wmId - ID_TRAY_PROFILES_START;

            if (index < presets.size()) {
                if (g_Config.LoadPreset(presets[index], g_Settings)) {
                    g_Logger.AddLog("[Tray] Swapping to: " + presets[index] + ". Reconnecting...", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                    g_Discord.Shutdown();

                    // Delay reconnection to allow Discord time to process the close
                    std::thread([]() {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        if (strlen(g_Settings.clientId) > 10) {
                            g_Discord.Initialize(g_Settings.clientId);
                        }
                        }).detach();
                }
            }
        }

        if (wmId == ID_TRAY_EXIT) g_ExitRequested = true;
        else if (wmId == ID_TRAY_SHOW) {
            ShowWindow(hWnd, SW_SHOW);
            SetForegroundWindow(hWnd);
        }
        return 0;
    }

    case WM_NCHITTEST: {
        // Allows window dragging from the top bar since we are using WS_POPUP
        LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
        if (hit == HTCLIENT) {
            POINT pt; pt.x = LOWORD(lParam); pt.y = HIWORD(lParam);
            ScreenToClient(hWnd, &pt);
            RECT rect; GetClientRect(hWnd, &rect);
            if (pt.y < 30 && pt.x < (rect.right - rect.left - 50)) return HTCAPTION;
        }
        return hit;
    }

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

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

/* Helper: Creates the D3D11 device and swapchain */
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
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

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    if (SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
