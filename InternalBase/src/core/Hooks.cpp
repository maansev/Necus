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
#include "../sdk/entity/EntityManager.h"
#include "../sdk/utils/Globals.h"
#include "../sdk/memory/Offsets.h"
#include "../sdk/memory/PatternScan.h"
#include "../feature/misc/Misc.h"
#include "../feature/misc/skin_changer.h"
#include "../feature/misc/glove_changer.h"
#include <wingdi.h>

static bool g_NightModeActive = false;
static WORD g_OriginalGamma[3][256];

#pragma comment(lib, "d3d11.lib")

ID3D11Device* g_Device = nullptr;
static ID3D11DeviceContext* g_Context = nullptr;
static ID3D11RenderTargetView* g_RTV = nullptr;
static HWND                    g_Window = nullptr;
static bool                    g_Init = false;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ─── BHop (cs2-dumper 2026-06-03) ────────────────────────────────────────────
static constexpr ptrdiff_t BUTTON_JUMP = 0x2064FA0;
static constexpr uint32_t  BTN_PRESSED = 0x10001u;
static constexpr uint32_t  BTN_RELEASED = 0x100u;
static bool s_bhop_was_ground = false;
static bool s_bhop_need_release = false;

// ─── SDL hooks ───────────────────────────────────────────────────────────────
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
        if (type == 0x400u) { // SDL_MOUSEMOTION
            *(int32_t*)((uint8_t*)event + 28) = 0;
            *(int32_t*)((uint8_t*)event + 32) = 0;
            *(int32_t*)((uint8_t*)event + 36) = 0;
            *(int32_t*)((uint8_t*)event + 40) = 0;
        }
    }
    return result;
}

// ─── Draw model placeholder ──────────────────────────────────────────────────
using DrawModelFn = void(__fastcall*)(void*, void*, void*, int, int, int, int);
DrawModelFn oDrawModel = nullptr;
void __fastcall hkDrawModel(void* t, void* e, void* r, int f, int u1, int u2, int u3) {
    oDrawModel(t, e, r, f, u1, u2, u3);
}

// ─── GetRawInputData hook (блокирует движение камеры при открытом меню) ──────
using GetRawInputDataFn = UINT(__stdcall*)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
GetRawInputDataFn oGetRawInputData = nullptr;
UINT __stdcall hkGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand,
    LPVOID pData, PUINT pcbSize, UINT cbSizeHeader)
{
    UINT result = oGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
    if (Menu::IsOpen && pData && uiCommand == RID_INPUT) {
        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(pData);
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            raw->data.mouse.lLastX = 0;
            raw->data.mouse.lLastY = 0;
        }
    }
    return result;
}


// ─── WndProc ─────────────────────────────────────────────────────────────────
LRESULT __stdcall Hooks::hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (Menu::IsOpen) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
        if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) return true;
        if (msg == WM_INPUT) return true;
        if (ImGui::GetIO().WantCaptureKeyboard &&
            (msg == WM_KEYDOWN || msg == WM_KEYUP ||
                msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_CHAR))
            return true;
    }
    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

// ─── CreateMove (не используется) ────────────────────────────────────────────
bool __stdcall Hooks::hkCreateMove(float frameTime, void* cmd) {
    return oCreateMove(frameTime, cmd);
}

// ─── FrameStageNotify ────────────────────────────────────────────────────────
using FrameStageNotifyFn = void(__fastcall*)(void*, int);

void __fastcall hkFrameStageNotify(void* ecx, int stage)
{
    Hooks::oFrameStageNotify(ecx, stage);

    if (stage == 0) {
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (client) {
            uintptr_t jumpAddr = client + BUTTON_JUMP;
            bool spaceHeld = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

            if (!Globals::misc_bhop || !spaceHeld) {
                *(uint32_t*)jumpAddr = BTN_RELEASED;
                s_bhop_was_ground = false;
                s_bhop_need_release = false;
            }
            else {
                __try {
                    auto* pawn = EntityManager::Get().GetLocalPawn();
                    if (pawn && pawn->IsAlive()) {
                        bool onGround = (*(int*)((uintptr_t)pawn + Offsets::m_fFlags) & 1) != 0;
                        if (onGround) {
                            if (!s_bhop_was_ground || s_bhop_need_release) {
                                *(uint32_t*)jumpAddr = BTN_RELEASED;
                                s_bhop_need_release = false;
                            }
                            else {
                                *(uint32_t*)jumpAddr = BTN_PRESSED;
                                s_bhop_need_release = true;
                            }
                        }
                        else {
                            *(uint32_t*)jumpAddr = BTN_RELEASED;
                            s_bhop_need_release = false;
                        }
                        s_bhop_was_ground = onGround;
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                    *(uint32_t*)(Memory::GetModuleBase("client.dll") + BUTTON_JUMP) = BTN_RELEASED;
                    s_bhop_was_ground = false;
                    s_bhop_need_release = false;
                }
            }
        }
    }

    if (stage == 7) {
        c_skin_changer::get().run(stage);
        c_glove_changer::get().run(stage);
    }
}

// ─── Present ─────────────────────────────────────────────────────────────────
HRESULT __stdcall Hooks::hkPresent(IDXGISwapChain* swapChain, UINT sync, UINT flags)
{
    if (!g_Init) {
        if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_Device)))
            return oPresent(swapChain, sync, flags);

        g_Device->GetImmediateContext(&g_Context);

        DXGI_SWAP_CHAIN_DESC sd{};
        swapChain->GetDesc(&sd);
        g_Window = sd.OutputWindow;

        ID3D11Texture2D* bb = nullptr;
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
        g_Device->CreateRenderTargetView(bb, nullptr, &g_RTV);
        bb->Release();

        oWndProc = (WNDPROC)SetWindowLongPtr(g_Window, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui_ImplWin32_Init(g_Window);
        ImGui_ImplDX11_Init(g_Device, g_Context);
        NecusLoadFontEmbedded(18.0f);
        NecusSetLogo(LoadNecusLogo(g_Device));

        g_Init = true;
    }

    EntityManager::Get().Update();

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (client)
        memcpy(&Globals::ViewMatrix,
            reinterpret_cast<void*>(client + Offsets::dwViewMatrix),
            sizeof(Globals::ViewMatrix));

    if (GetAsyncKeyState(VK_INSERT) & 1) {
        Menu::IsOpen = !Menu::IsOpen;
        if (Menu::IsOpen) {
            ::ShowCursor(TRUE);
            ::ClipCursor(nullptr);
            if (oSDL_SetRelativeMouseMode)       oSDL_SetRelativeMouseMode(0);
            if (oSDL_SetWindowRelativeMouseMode) oSDL_SetWindowRelativeMouseMode(nullptr, 0);
        }
        else {
            ::ShowCursor(FALSE);
            if (oSDL_SetRelativeMouseMode)       oSDL_SetRelativeMouseMode(1);
            if (oSDL_SetWindowRelativeMouseMode) oSDL_SetWindowRelativeMouseMode(nullptr, 1);
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (Menu::IsOpen) Menu::Render();
    Visuals::Render();
    ImGui::Render();
    g_Context->OMSetRenderTargets(1, &g_RTV, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

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

// ─── Setup ───────────────────────────────────────────────────────────────────
void Hooks::Setup()
{
    chams::Init();
    if (MH_Initialize() != MH_OK) return;

    {
        uintptr_t addr = Memory::PatternScan("client.dll",
            "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC 30 49 8B F8");
        if (addr) MH_CreateHook(reinterpret_cast<void*>(addr),
            &chams::DrawArrayHook, (void**)&oDrawArray);
    }

    {
        uintptr_t fsn = Memory::PatternScan("client.dll",
            "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC 40 48 8B F9 33 ED");
        if (fsn) MH_CreateHook(reinterpret_cast<void*>(fsn),
            &hkFrameStageNotify,
            reinterpret_cast<void**>(&oFrameStageNotify));
    }

    // GetRawInputData — блокирует движение камеры через raw input
    {
        void* fn = GetProcAddress(GetModuleHandleA("user32.dll"), "GetRawInputData");
        if (fn) MH_CreateHook(fn, &hkGetRawInputData, (void**)&oGetRawInputData);
    }

    {
        HMODULE sdl = GetModuleHandleA("SDL2.dll");
        if (!sdl) sdl = GetModuleHandleA("SDL3.dll");
        if (sdl) {
            if (auto* fn = GetProcAddress(sdl, "SDL_SetRelativeMouseMode"))
                MH_CreateHook(fn, &hkSDL_SetRelativeMouseMode,
                    (void**)&oSDL_SetRelativeMouseMode);
            if (auto* fn = GetProcAddress(sdl, "SDL_SetWindowRelativeMouseMode"))
                MH_CreateHook(fn, &hkSDL_SetWindowRelativeMouseMode,
                    (void**)&oSDL_SetWindowRelativeMouseMode);
            if (auto* fn = GetProcAddress(sdl, "SDL_GetRelativeMouseState"))
                MH_CreateHook(fn, &hkSDL_GetRelativeMouseState,
                    (void**)&oSDL_GetRelativeMouseState);
            if (auto* fn = GetProcAddress(sdl, "SDL_PollEvent"))
                MH_CreateHook(fn, &hkSDL_PollEvent,
                    (void**)&oSDL_PollEvent);
        }
    }

    {
        HMODULE cm = GetModuleHandleA("client.dll");
        if (cm) {
            using CI_t = void* (*)(const char*, int*);
            auto CI = (CI_t)GetProcAddress(cm, "CreateInterface");
            if (CI) {
                void* mr = CI("VModelRender", nullptr);
                if (mr) MH_CreateHook((*reinterpret_cast<void***>(mr))[21],
                    &hkDrawModel, (void**)&oDrawModel);
            }
        }
    }

    {
        WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"NecusDX";
        RegisterClassExW(&wc);

        HWND hw = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
            0, 0, 100, 100, nullptr, nullptr,
            wc.hInstance, nullptr);

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 1; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hw; sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* sc = nullptr; ID3D11Device* dv = nullptr;
        ID3D11DeviceContext* cx = nullptr; D3D_FEATURE_LEVEL fl;

        if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,
            D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &sd, &sc, &dv, &fl, &cx)))
        {
            void** vt = *reinterpret_cast<void***>(sc);
            MH_CreateHook(vt[8], &hkPresent, reinterpret_cast<void**>(&oPresent));
            MH_EnableHook(MH_ALL_HOOKS);
            sc->Release(); dv->Release(); cx->Release();
        }
        DestroyWindow(hw);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }
}

// ─── Destroy ─────────────────────────────────────────────────────────────────
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
    chams::Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_RTV)     g_RTV->Release();
    if (g_Context) g_Context->Release();
    if (g_Device)  g_Device->Release();
    g_Init = false;
}