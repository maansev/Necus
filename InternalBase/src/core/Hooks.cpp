#define _CRT_SECURE_NO_WARNINGS
#include "Hooks.h"
#include "../../ext/imgui/imgui.h"
#include "../../ext/imgui/imgui_impl_win32.h"
#include "../../ext/imgui/imgui_impl_dx11.h"
#include "../../ext/minhook/MinHook.h"
#include "../menu/Menu.h"
#include "../menu/LogoLoader_D3D11.h"
#include "../feature/visuals/Visuals.h"
#include "../feature/visuals/chams/Chams.h"
#include "../feature/combat/legitbot/Aimbot.h"
#include "../sdk/entity/EntityManager.h"
#include "../sdk/utils/Globals.h"
#include "../sdk/memory/Offsets.h"
#include "../sdk/memory/PatternScan.h"
#include "../feature/misc/Misc.h"
#include "../feature/misc/inventory_changer.h"
#include "../feature/misc/NoFlash.h"
#include "../feature/misc/NoSmoke.h"
#include "../feature/misc/Viewmodel.h"
#include "../feature/misc/Movement.h"
#include "../sdk/utils/Log.h"
#include <wingdi.h>

static bool g_NightModeActive = false;
static WORD g_OriginalGamma[3][256];

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#include <dxgi1_3.h>

ID3D11Device* g_Device = nullptr;
static ID3D11DeviceContext* g_Context = nullptr;
static ID3D11RenderTargetView* g_RTV = nullptr;
static HWND g_Window = nullptr;
static bool g_Init = false;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// Button offsets
static constexpr ptrdiff_t BUTTON_JUMP = 0x2065FA0;
static constexpr ptrdiff_t BUTTON_MOVELEFT = 0x2065DF0;
static constexpr ptrdiff_t BUTTON_MOVERIGHT = 0x2065E80;
static constexpr ptrdiff_t BUTTON_DUCK = 0x2066030;
static constexpr ptrdiff_t DW_VIEW_ANGLES = 0x23568C8;
static constexpr uint32_t BTN_PRESSED = 0x10001u;
static constexpr uint32_t BTN_RELEASED = 0x100u;

// Movement states
static bool  s_bhop_was_ground = false;
static bool  s_bhop_roll_pass = true;
static bool  s_duck_forced = false;
static float s_prevYaw = 0.f;
static bool  s_yawInit = false;

// Menu cursor lock
static float s_frozenAngles[3] = {};
static bool  s_anglesValid = false;
static bool  s_prevMenuOpen = false;

// Aspect ratio
static UINT s_arSrcW = 0, s_arBbW = 0, s_arBbH = 0;
static bool s_arWasEnabled = false;

// SDL hooks
using SDL_SetRelativeMouseModeFn = int(__cdecl*)(int);
SDL_SetRelativeMouseModeFn oSDL_SetRelativeMouseMode = nullptr;

int __cdecl hkSDL_SetRelativeMouseMode(int enabled) {
    if (Menu::IsOpen) return 0;
    return oSDL_SetRelativeMouseMode(enabled);
}

using SDL_SetWindowRelativeMouseModeFn = int(__cdecl*)(void*, int);
SDL_SetWindowRelativeMouseModeFn oSDL_SetWindowRelativeMouseMode = nullptr;

int __cdecl hkSDL_SetWindowRelativeMouseMode(void* window, int enabled) {
    if (Menu::IsOpen) return 0;
    return oSDL_SetWindowRelativeMouseMode(window, enabled);
}

using SDL_GetRelativeMouseStateFn = uint32_t(__cdecl*)(void*, void*);
SDL_GetRelativeMouseStateFn oSDL_GetRelativeMouseState = nullptr;

uint32_t __cdecl hkSDL_GetRelativeMouseState(void* x, void* y) {
    uint32_t result = oSDL_GetRelativeMouseState(x, y);
    if (Menu::IsOpen) {
        if (x) *(int32_t*)x = 0;
        if (y) *(int32_t*)y = 0;
    }
    return result;
}

using SDL_PollEventFn = int(__cdecl*)(void*);
SDL_PollEventFn oSDL_PollEvent = nullptr;

int __cdecl hkSDL_PollEvent(void* event) {
    int result = oSDL_PollEvent(event);
    if (result && event && Menu::IsOpen) {
        uint32_t type = *(uint32_t*)event;
        if (type == 0x400u) {
            *(int32_t*)((uint8_t*)event + 28) = 0;
            *(int32_t*)((uint8_t*)event + 32) = 0;
            *(int32_t*)((uint8_t*)event + 36) = 0;
            *(int32_t*)((uint8_t*)event + 40) = 0;
        }
        else if (type == 0x401u || type == 0x402u) {
            *(uint32_t*)event = 0;
        }
        else if (type == 0x403u) {
            *(int32_t*)((uint8_t*)event + 16) = 0;
            *(int32_t*)((uint8_t*)event + 20) = 0;
        }
    }
    return result;
}

using GetRawInputDataFn = UINT(__stdcall*)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
GetRawInputDataFn oGetRawInputData = nullptr;

UINT __stdcall hkGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader)
{
    UINT result = oGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
    if (pData && uiCommand == RID_INPUT) {
        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(pData);
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            if (Menu::IsOpen) {
                raw->data.mouse.lLastX = 0;
                raw->data.mouse.lLastY = 0;
                raw->data.mouse.usButtonFlags = 0;
                raw->data.mouse.usButtonData = 0;
            }
            else if (Globals::aimbot_enabled && Globals::aimbot_lock_mouse && Aimbot::HasTarget()) {
                raw->data.mouse.lLastX = 0;
                raw->data.mouse.lLastY = 0;
            }
        }
    }
    return result;
}

LRESULT __stdcall Hooks::hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (Menu::IsOpen) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return 1;

        if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) return 0;
        if (msg == WM_INPUT) return 0;

        if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_CHAR || msg == WM_SYSCHAR) {
            if (wParam == VK_INSERT)
                return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
            return 0;
        }
    }
    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

bool __stdcall Hooks::hkCreateMove(float frameTime, void* cmd) {
    return oCreateMove(frameTime, cmd);
}

using RenderFlashbangOverlayFn = void(__fastcall*)(void*, void*, void*, void*, void*);
static RenderFlashbangOverlayFn oRenderFlashbangOverlay = nullptr;

void __fastcall hkRenderFlashbangOverlay(void* a1, void* a2, void* a3, void* a4, void* a5) {
    if (Globals::misc_noflash) return;
    oRenderFlashbangOverlay(a1, a2, a3, a4, a5);
}

using LevelInitFn = void* (__fastcall*)(void*, const char*);
static LevelInitFn oLevelInit = nullptr;

void* __fastcall hkLevelInit(void* thisptr, const char* szNewMap) {
    Log::Write("LevelInit: map=%s", szNewMap ? szNewMap : "(null)");
    return oLevelInit(thisptr, szNewMap);
}

using FrameStageNotifyFn = void(__fastcall*)(void*, int);

static bool SafeCallOrigFSN(void* ecx, int stage) {
    __try { Hooks::oFrameStageNotify(ecx, stage); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Write("oFSN CRASH stage=%d ecx=0x%p", stage, ecx);
        return false;
    }
}

void __fastcall hkFrameStageNotify(void* ecx, int stage)
{
    if (!SafeCallOrigFSN(ecx, stage)) return;

    if (stage == 0) {
        Log::Checkpoint("FSN0:Aimbot");
        Aimbot::Run(0);

        Log::Checkpoint("FSN0:Viewmodel");
        __try { Viewmodel::Update(0); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        Log::Checkpoint("FSN0:movement");

        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        __try {
            uintptr_t localController = *reinterpret_cast<uintptr_t*>(client + Offsets::dwLocalPlayerController);
            if (!localController || localController < 0x10000ULL) return;

            auto* pawn = EntityManager::Get().GetLocalPawn();
            if (!pawn || (uintptr_t)pawn < 0x10000ULL || (uintptr_t)pawn > 0x7FFFFFFFFFFFULL) return;

            if (Offsets::m_fFlags < 0x100 || Offsets::m_fFlags > 0x2000 ||
                Offsets::m_MoveType < 0x100 || Offsets::m_MoveType > 0x2000) return;

            if (!pawn->IsAlive()) {
                s_bhop_was_ground = false;
                s_yawInit = false;
                if (s_duck_forced) {
                    *(uint32_t*)(client + BUTTON_DUCK) = BTN_RELEASED;
                    s_duck_forced = false;
                }
                return;
            }

            int flags = *(int*)((uintptr_t)pawn + Offsets::m_fFlags);
            bool onGround = (flags & 1) != 0;
            uint8_t moveType = *(uint8_t*)((uintptr_t)pawn + Offsets::m_MoveType);
            bool moveOk = (moveType != 9) && (moveType != 8);

            // BHOP
            {
                uintptr_t jumpAddr = client + BUTTON_JUMP;
                bool spaceHeld = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

                if (!Globals::misc_bhop || !moveOk) {
                    s_bhop_was_ground = false;
                }
                else if (!spaceHeld) {
                    *(uint32_t*)jumpAddr = BTN_RELEASED;
                    s_bhop_was_ground = false;
                }
                else {
                    if (onGround && !s_bhop_was_ground)
                        s_bhop_roll_pass = (Globals::misc_bhop_mode == 0) || ((rand() % 100) < Globals::misc_bhop_chance);

                    *(uint32_t*)jumpAddr = (onGround && s_bhop_roll_pass) ? BTN_PRESSED : BTN_RELEASED;
                    s_bhop_was_ground = onGround;
                }
            }

            // Autostrafer + Infinite Duck
            Movement::RunAutostrafer();
            Movement::RunInfiniteDuck();

        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            s_bhop_was_ground = false;
            s_yawInit = false;
            s_duck_forced = false;
        }
    }

    if (stage == 5) {
        Log::Checkpoint("FSN5:NoFlash");
        __try { NoFlash::Update(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        Log::Checkpoint("FSN5:Viewmodel");
        __try { Viewmodel::Update(5); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        Log::Checkpoint("FSN5:OnRenderStart");
        __try { Visuals::OnRenderStart(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (stage == 6) {
        Log::Checkpoint("FSN6:invchanger");
        __try { c_inventory_changer::get().run(6); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (stage == 7) {
        Log::Checkpoint("FSN7:OnRenderEnd");
        __try { Visuals::OnRenderEnd(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        Log::Checkpoint("FSN7:Aimbot");
        Aimbot::Run(7);
    }
}

static void RebuildRTV(IDXGISwapChain* sc) {
    if (g_RTV) { g_RTV->Release(); g_RTV = nullptr; }
    ID3D11Texture2D* bb = nullptr;
    if (SUCCEEDED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb)) && bb) {
        g_Device->CreateRenderTargetView(bb, nullptr, &g_RTV);
        bb->Release();
    }
}

HRESULT __stdcall Hooks::hkPresent(IDXGISwapChain* swapChain, UINT sync, UINT flags)
{
    if (!g_Init) {
        if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_Device)))
            return oPresent(swapChain, sync, flags);

        g_Device->GetImmediateContext(&g_Context);
        DXGI_SWAP_CHAIN_DESC sd{};
        swapChain->GetDesc(&sd);
        g_Window = sd.OutputWindow;

        RebuildRTV(swapChain);

        oWndProc = (WNDPROC)SetWindowLongPtr(g_Window, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui_ImplWin32_Init(g_Window);
        ImGui_ImplDX11_Init(g_Device, g_Context);
        NecusLoadFontEmbedded(18.0f);
        NecusSetLogo(LoadNecusLogo(g_Device));
        Chams::Init();
        g_Init = true;
    }

    {
        static UINT s_rtvW = 0, s_rtvH = 0;
        DXGI_SWAP_CHAIN_DESC sd{};
        if (SUCCEEDED(swapChain->GetDesc(&sd))) {
            UINT w = sd.BufferDesc.Width, h = sd.BufferDesc.Height;
            if (w != s_rtvW || h != s_rtvH || !g_RTV) {
                RebuildRTV(swapChain);
                ImGui_ImplDX11_InvalidateDeviceObjects();
                s_rtvW = w; s_rtvH = h;
                s_arSrcW = 0; s_arBbW = 0; s_arBbH = 0; s_arWasEnabled = false;
            }
        }
    }

    // Cursor lock while menu open
    __try {
        uintptr_t c0 = Memory::GetModuleBase("client.dll");
        if (c0) {
            auto* va = reinterpret_cast<float*>(c0 + DW_VIEW_ANGLES);
            if (Menu::IsOpen) {
                if (!s_prevMenuOpen) {
                    s_frozenAngles[0] = va[0];
                    s_frozenAngles[1] = va[1];
                    s_frozenAngles[2] = va[2];
                    s_anglesValid = true;
                }
                if (s_anglesValid) {
                    va[0] = s_frozenAngles[0];
                    va[1] = s_frozenAngles[1];
                    va[2] = s_frozenAngles[2];
                }
            }
            else {
                s_anglesValid = false;
            }
            s_prevMenuOpen = Menu::IsOpen;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    // Основные обновления (оптимизировано)
    EntityManager::Get().Update();
    Visuals::UpdateRemovals();
    NoFlash::Update();

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (client) {
        uintptr_t smList = *reinterpret_cast<uintptr_t*>(client + Offsets::dwEntityList);
        NoSmoke::Update(smList);
    }

    Menu::UpdateBinds();
    Misc::RunAutoAccept(g_Window);

    if (client) {
        memcpy(&Globals::ViewMatrix, reinterpret_cast<void*>(client + Offsets::dwViewMatrix), sizeof(Globals::ViewMatrix));
    }

    if (GetAsyncKeyState(VK_INSERT) & 1) {
        Menu::IsOpen = !Menu::IsOpen;

        if (Menu::IsOpen) {
            if (oSDL_SetRelativeMouseMode) oSDL_SetRelativeMouseMode(0);
            if (oSDL_SetWindowRelativeMouseMode) oSDL_SetWindowRelativeMouseMode(nullptr, 0);
            ::ClipCursor(nullptr);
            ::ShowCursor(TRUE);

            if (g_Window) {
                RECT rc; GetClientRect(g_Window, &rc);
                POINT pt = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
                ClientToScreen(g_Window, &pt);
                SetCursorPos(pt.x, pt.y);
            }
        }
        else {
            ::ShowCursor(FALSE);
            if (oSDL_SetRelativeMouseMode) oSDL_SetRelativeMouseMode(1);
            if (oSDL_SetWindowRelativeMouseMode) oSDL_SetWindowRelativeMouseMode(nullptr, 1);
        }
    }

    // Aspect Ratio
    {
        bool needUpdate = Globals::misc_aspect_ratio_enabled || s_arWasEnabled;
        if (needUpdate) {
            IDXGISwapChain2* sc2 = nullptr;
            if (SUCCEEDED(swapChain->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&sc2)) && sc2) {
                DXGI_SWAP_CHAIN_DESC sd{};
                swapChain->GetDesc(&sd);
                s_arBbW = sd.BufferDesc.Width;
                s_arBbH = sd.BufferDesc.Height;
                s_arSrcW = s_arBbW;

                if (Globals::misc_aspect_ratio_enabled) {
                    UINT srcW = (UINT)(s_arBbH * Globals::misc_aspect_ratio);
                    if (srcW < 16) srcW = 16;
                    if (srcW > s_arBbW) srcW = s_arBbW;
                    s_arSrcW = srcW;
                }
                sc2->SetSourceSize(s_arSrcW, s_arBbH);
                sc2->Release();
            }
        }
        s_arWasEnabled = Globals::misc_aspect_ratio_enabled;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    if (Globals::misc_aspect_ratio_enabled && s_arSrcW > 0 && s_arSrcW < s_arBbW)
        ImGui::GetIO().DisplaySize.x = (float)s_arSrcW;

    ImGui::NewFrame();

    if (Menu::IsOpen) Menu::Render();
    Visuals::Render();
    Menu::RenderOverlays();
    ImGui::Render();

    if (g_RTV) {
        g_Context->OMSetRenderTargets(1, &g_RTV, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    // Nightmode
    if (Globals::misc_nightmode) {
        if (!g_NightModeActive) {
            HDC dc = GetDC(g_Window);
            GetDeviceGammaRamp(dc, g_OriginalGamma);
            ReleaseDC(g_Window, dc);
            g_NightModeActive = true;
        }
        HDC dc = GetDC(g_Window);
        WORD ramp[3][256];
        float br = Globals::misc_nightmode_brightness;
        for (int i = 0; i < 256; ++i) {
            WORD v = (WORD)min(65535.f, i * 256.f * br);
            ramp[0][i] = ramp[1][i] = ramp[2][i] = v;
        }
        SetDeviceGammaRamp(dc, ramp);
        ReleaseDC(g_Window, dc);
    }
    else if (g_NightModeActive) {
        HDC dc = GetDC(g_Window);
        SetDeviceGammaRamp(dc, g_OriginalGamma);
        ReleaseDC(g_Window, dc);
        g_NightModeActive = false;
    }

    return oPresent(swapChain, sync, flags);
}

void Hooks::Setup()
{
    Log::Init();
    Log::Write("Hooks::Setup start");

    if (MH_Initialize() != MH_OK) {
        Log::Write("MH_Initialize FAILED");
        return;
    }

    NoFlash::Init();

    {
        uintptr_t fsn = Memory::PatternScan("client.dll", "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC 40 48 8B F9 33 ED");
        if (fsn) MH_CreateHook(reinterpret_cast<void*>(fsn), &hkFrameStageNotify, reinterpret_cast<void**>(&oFrameStageNotify));
    }

    {
        uintptr_t fn = Memory::PatternScan("client.dll", "48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 B4 24");
        if (fn) MH_CreateHook(reinterpret_cast<void*>(fn), &hkRenderFlashbangOverlay, reinterpret_cast<void**>(&oRenderFlashbangOverlay));
    }

    {
        uintptr_t fn = Memory::PatternScan("client.dll", "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B FA 48 8B F1 48 85 D2 0F 84");
        if (fn) MH_CreateHook(reinterpret_cast<void*>(fn), &hkLevelInit, reinterpret_cast<void**>(&oLevelInit));
    }

    {
        uintptr_t fn = Memory::PatternScan("scenesystem.dll", "48 8B C4 48 89 50 10 53 41 55 41 56 48 81 EC ? ? ? ? 4D 63 F1");
        if (fn) MH_CreateHook(reinterpret_cast<void*>(fn), &Chams::HookDrawArray, reinterpret_cast<void**>(&Chams::oDrawArray));
    }

    {
        void* fn = GetProcAddress(GetModuleHandleA("user32.dll"), "GetRawInputData");
        if (fn) MH_CreateHook(fn, &hkGetRawInputData, (void**)&oGetRawInputData);
    }

    {
        HMODULE sdl = GetModuleHandleA("SDL2.dll");
        if (!sdl) sdl = GetModuleHandleA("SDL3.dll");
        if (sdl) {
            if (auto* fn = GetProcAddress(sdl, "SDL_SetRelativeMouseMode")) MH_CreateHook(fn, &hkSDL_SetRelativeMouseMode, (void**)&oSDL_SetRelativeMouseMode);
            if (auto* fn = GetProcAddress(sdl, "SDL_SetWindowRelativeMouseMode")) MH_CreateHook(fn, &hkSDL_SetWindowRelativeMouseMode, (void**)&oSDL_SetWindowRelativeMouseMode);
            if (auto* fn = GetProcAddress(sdl, "SDL_GetRelativeMouseState")) MH_CreateHook(fn, &hkSDL_GetRelativeMouseState, (void**)&oSDL_GetRelativeMouseState);
            if (auto* fn = GetProcAddress(sdl, "SDL_PollEvent")) MH_CreateHook(fn, &hkSDL_PollEvent, (void**)&oSDL_PollEvent);
        }
    }

    {
        WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"NecusDX";
        RegisterClassExW(&wc);

        HWND hw = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hw;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* sc = nullptr;
        ID3D11Device* dv = nullptr;
        ID3D11DeviceContext* cx = nullptr;
        D3D_FEATURE_LEVEL fl;

        if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &sd, &sc, &dv, &fl, &cx))) {
            void** vt = *reinterpret_cast<void***>(sc);
            MH_CreateHook(vt[8], &hkPresent, reinterpret_cast<void**>(&oPresent));
            MH_EnableHook(MH_ALL_HOOKS);
            sc->Release();
            dv->Release();
            cx->Release();
        }

        DestroyWindow(hw);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }
}

void Hooks::Destroy()
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (g_Window && oWndProc)
        SetWindowLongPtr(g_Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));

    if (g_NightModeActive) {
        HDC dc = GetDC(g_Window);
        SetDeviceGammaRamp(dc, g_OriginalGamma);
        ReleaseDC(g_Window, dc);
    }

    if (!g_Init) return;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_RTV) g_RTV->Release();
    if (g_Context) g_Context->Release();
    if (g_Device) g_Device->Release();

    g_Init = false;
}