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

// ─── Button offsets (cs2-dumper 2026-06-03) ──────────────────────────────────
static constexpr ptrdiff_t BUTTON_JUMP = 0x2064FA0;
static constexpr ptrdiff_t BUTTON_MOVELEFT = 0x2064DF0;
static constexpr ptrdiff_t BUTTON_MOVERIGHT = 0x2064E80;
static constexpr ptrdiff_t BUTTON_DUCK = 0x2065030;
static constexpr ptrdiff_t DW_VIEW_ANGLES = 0x23558B8;
static constexpr uint32_t  BTN_PRESSED = 0x10001u;
static constexpr uint32_t  BTN_RELEASED = 0x100u;

// ─── BHop state ──────────────────────────────────────────────────────────────
static bool s_bhop_was_ground = false;
static bool s_bhop_need_release = false;

// ─── Auto strafe — yaw-delta approach ────────────────────────────────────────
// К моменту stage 0, SDL уже обновил dwViewAngles. Читаем разницу с прошлым
// фреймом — надёжнее накопления raw mouse delta.
static float s_prevYaw = 0.f;
static bool  s_yawInit = false;

// ─── Camera lock state ───────────────────────────────────────────────────────
static float s_savedViewAngles[3] = {};
static float s_savedEyeAngles[3] = {};
static bool  s_anglesSaved = false;

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
        if (type == 0x400u) {
            // SDL_MOUSEMOTION — zero movement deltas
            *(int32_t*)((uint8_t*)event + 28) = 0;
            *(int32_t*)((uint8_t*)event + 32) = 0;
            *(int32_t*)((uint8_t*)event + 36) = 0;
            *(int32_t*)((uint8_t*)event + 40) = 0;
        }
        else if (type == 0x401u || type == 0x402u) {
            // SDL_MOUSEBUTTONDOWN / SDL_MOUSEBUTTONUP — swallow so game doesn't shoot
            *(uint32_t*)event = 0;
        }
        else if (type == 0x403u) {
            // SDL_MOUSEWHEEL — zero scroll so game camera doesn't zoom
            *(int32_t*)((uint8_t*)event + 16) = 0;
            *(int32_t*)((uint8_t*)event + 20) = 0;
        }
    }
    return result;
}

// ─── DrawModel placeholder ───────────────────────────────────────────────────
using DrawModelFn = void(__fastcall*)(void*, void*, void*, int, int, int, int);
DrawModelFn oDrawModel = nullptr;
void __fastcall hkDrawModel(void* t, void* e, void* r, int f, int u1, int u2, int u3) {
    oDrawModel(t, e, r, f, u1, u2, u3);
}

// ─── GetRawInputData ─────────────────────────────────────────────────────────
using GetRawInputDataFn = UINT(__stdcall*)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
GetRawInputDataFn oGetRawInputData = nullptr;
UINT __stdcall hkGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand,
    LPVOID pData, PUINT pcbSize, UINT cbSizeHeader)
{
    UINT result = oGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
    if (pData && uiCommand == RID_INPUT) {
        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(pData);
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            if (Menu::IsOpen) {
                raw->data.mouse.lLastX = 0;
                raw->data.mouse.lLastY = 0;
                raw->data.mouse.usButtonFlags = 0; // block clicks reaching game
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

// ─── WndProc ─────────────────────────────────────────────────────────────────
LRESULT __stdcall Hooks::hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (Menu::IsOpen) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
        if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) return true;
        if (msg == WM_INPUT)  return true;
        if (ImGui::GetIO().WantCaptureKeyboard &&
            (msg == WM_KEYDOWN || msg == WM_KEYUP ||
                msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_CHAR))
            return true;
    }
    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

bool __stdcall Hooks::hkCreateMove(float frameTime, void* cmd) {
    return oCreateMove(frameTime, cmd);
}

// ─── FrameStageNotify ────────────────────────────────────────────────────────
using FrameStageNotifyFn = void(__fastcall*)(void*, int);

void __fastcall hkFrameStageNotify(void* ecx, int stage)
{
    Hooks::oFrameStageNotify(ecx, stage);

    // ── Stage 0 ───────────────────────────────────────────────────────────────
    if (stage == 0) {
        // Camera lock: restore angles at the very start so game never processes mouse delta
        if (Menu::IsOpen && s_anglesSaved) {
            uintptr_t c0 = Memory::GetModuleBase("client.dll");
            if (c0) {
                __try {
                    float* va = reinterpret_cast<float*>(c0 + DW_VIEW_ANGLES);
                    va[0] = s_savedViewAngles[0]; va[1] = s_savedViewAngles[1]; va[2] = s_savedViewAngles[2];
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }

        // Aimbot runs first (sets aim angles before movement processing)
        Aimbot::Run(0);

        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        __try {
            auto* pawn = EntityManager::Get().GetLocalPawn();
            if (!pawn || !pawn->IsAlive()) {
                s_bhop_was_ground = false; s_bhop_need_release = false; s_yawInit = false;
                return;
            }

            bool onGround = (*(int*)((uintptr_t)pawn + Offsets::m_fFlags) & 1) != 0;

            // ── BHop ─────────────────────────────────────────────────────────
            {
                uintptr_t jumpAddr = client + BUTTON_JUMP;
                bool spaceHeld = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

                if (!Globals::misc_bhop) {
                    // Don't touch BUTTON_JUMP when disabled — let game handle input normally
                    s_bhop_was_ground = false; s_bhop_need_release = false;
                }
                else if (!spaceHeld) {
                    *(uint32_t*)jumpAddr = BTN_RELEASED;
                    s_bhop_was_ground = false; s_bhop_need_release = false;
                }
                else {
                    if (onGround) {
                        if (!s_bhop_was_ground || s_bhop_need_release) {
                            *(uint32_t*)jumpAddr = BTN_RELEASED;
                            s_bhop_need_release = false;
                        }
                        else {
                            bool doJump = (Globals::misc_bhop_mode == 0)
                                || ((rand() % 100) < Globals::misc_bhop_chance);
                            *(uint32_t*)jumpAddr = doJump ? BTN_PRESSED : BTN_RELEASED;
                            s_bhop_need_release = doJump;
                        }
                    }
                    else {
                        *(uint32_t*)jumpAddr = BTN_RELEASED;
                        s_bhop_need_release = false;
                    }
                    s_bhop_was_ground = onGround;
                }
            }

            // ── Auto Strafe (yaw-delta) ───────────────────────────────────────
            if (Globals::misc_autostrafer) {
                uintptr_t L = client + BUTTON_MOVELEFT;
                uintptr_t R = client + BUTTON_MOVERIGHT;

                float* vaPtr = reinterpret_cast<float*>(client + DW_VIEW_ANGLES);
                float  curYaw = vaPtr[1];
                if (!s_yawInit) { s_prevYaw = curYaw; s_yawInit = true; }

                float delta = curYaw - s_prevYaw;
                while (delta > 180.f) delta -= 360.f;
                while (delta < -180.f) delta += 360.f;
                s_prevYaw = curYaw;

                float smooth = Globals::misc_autostrafer_smooth < 0.1f ? 0.1f : Globals::misc_autostrafer_smooth;
                float threshold = 0.15f / smooth;

                // Track what we pressed so we only release OUR buttons, not the player's A/D
                static int s_strafeState = 0; // 0=none 1=left 2=right

                if (!onGround) {
                    if (delta > threshold) {
                        *(uint32_t*)R = BTN_PRESSED; *(uint32_t*)L = BTN_RELEASED;
                        s_strafeState = 2;
                    }
                    else if (delta < -threshold) {
                        *(uint32_t*)L = BTN_PRESSED; *(uint32_t*)R = BTN_RELEASED;
                        s_strafeState = 1;
                    }
                    else {
                        // Don't touch A/D — let player control freely
                        if (s_strafeState == 1) *(uint32_t*)L = BTN_RELEASED;
                        else if (s_strafeState == 2) *(uint32_t*)R = BTN_RELEASED;
                        s_strafeState = 0;
                    }
                }
                else {
                    // Release only what strafer pressed
                    if (s_strafeState == 1) *(uint32_t*)L = BTN_RELEASED;
                    else if (s_strafeState == 2) *(uint32_t*)R = BTN_RELEASED;
                    s_strafeState = 0;
                }
            }
            else {
                s_yawInit = false;
            }

            // ── Infinite Duck ─────────────────────────────────────────────────
            if (Globals::misc_infinite_duck)
                *(uint32_t*)(client + BUTTON_DUCK) = BTN_PRESSED;

        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            s_bhop_was_ground = false; s_bhop_need_release = false;
        }
    }

    // ── Stage 5: FRAME_RENDER_START — right before the frame is drawn ─────────
    if (stage == 5) {
        // No Flash: zero the flash alpha BEFORE the game renders the screen overlay
        NoFlash::Update();
        // Viewmodel editor: apply X/Y/Z/FOV before render
        Viewmodel::Update();
    }

    // ── Stage 6: FRAME_RENDER_END — inventory changer + camera-lock ──────────
    if (stage == 6) {
        c_inventory_changer::get().run(6);

        if (Menu::IsOpen) {
            uintptr_t client = Memory::GetModuleBase("client.dll");
            if (client && !s_anglesSaved) {
                __try {
                    float* va = reinterpret_cast<float*>(client + DW_VIEW_ANGLES);
                    s_savedViewAngles[0] = va[0];
                    s_savedViewAngles[1] = va[1];
                    s_savedViewAngles[2] = va[2];
                    auto* pawn = EntityManager::Get().GetLocalPawn();
                    if (pawn && pawn->IsAlive()) {
                        float* ea = reinterpret_cast<float*>((uintptr_t)pawn + Offsets::m_angEyeAngles);
                        s_savedEyeAngles[0] = ea[0]; s_savedEyeAngles[1] = ea[1]; s_savedEyeAngles[2] = ea[2];
                    }
                    s_anglesSaved = true;
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
        else {
            s_anglesSaved = false;
        }
    }

    // ── Stage 7: silent aimbot restore, camera-lock restore ──────────────────
    if (stage == 7) {
        // Silent aimbot: восстановить визуальный угол (пули уже пошли по нужному направлению)
        Aimbot::Run(7);

        // Camera lock: перезаписать ПОСЛЕ обработки ввода игрой
        if (Menu::IsOpen && s_anglesSaved) {
            uintptr_t client = Memory::GetModuleBase("client.dll");
            if (client) {
                __try {
                    float* va = reinterpret_cast<float*>(client + DW_VIEW_ANGLES);
                    va[0] = s_savedViewAngles[0]; va[1] = s_savedViewAngles[1]; va[2] = s_savedViewAngles[2];
                    auto* pawn = EntityManager::Get().GetLocalPawn();
                    if (pawn && pawn->IsAlive()) {
                        float* ea = reinterpret_cast<float*>((uintptr_t)pawn + Offsets::m_angEyeAngles);
                        ea[0] = s_savedEyeAngles[0]; ea[1] = s_savedEyeAngles[1]; ea[2] = s_savedEyeAngles[2];
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
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
    chams::Update();

    // No Flash — write every rendered frame so the game can't re-apply flash between FSN stages
    NoFlash::Update();

    // No Smoke — scan entity list for smoke projectiles and suppress them
    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (client) {
        __try {
            uintptr_t smList = *reinterpret_cast<uintptr_t*>(client + Offsets::dwEntityList);
            NoSmoke::Update(smList);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    Menu::UpdateBinds(); // apply bind logic every frame (Hold/Toggle/Always)

    client = Memory::GetModuleBase("client.dll");
    if (client)
        memcpy(&Globals::ViewMatrix,
            reinterpret_cast<void*>(client + Offsets::dwViewMatrix),
            sizeof(Globals::ViewMatrix));

    if (GetAsyncKeyState(VK_INSERT) & 1) {
        Menu::IsOpen = !Menu::IsOpen;
        if (Menu::IsOpen) {
            if (oSDL_SetRelativeMouseMode)       oSDL_SetRelativeMouseMode(0);
            if (oSDL_SetWindowRelativeMouseMode) oSDL_SetWindowRelativeMouseMode(nullptr, 0);
            ::ClipCursor(nullptr);
            ::ShowCursor(TRUE);
            // Warp cursor to window centre so ImGui doesn't start at (0,0)
            if (g_Window) {
                RECT rc; GetClientRect(g_Window, &rc);
                POINT pt = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
                ClientToScreen(g_Window, &pt);
                SetCursorPos(pt.x, pt.y);
            }
            // Save view angles immediately on menu open so camera is frozen from this frame
            uintptr_t cOpen = Memory::GetModuleBase("client.dll");
            if (cOpen && !s_anglesSaved) {
                __try {
                    float* va = reinterpret_cast<float*>(cOpen + DW_VIEW_ANGLES);
                    s_savedViewAngles[0] = va[0]; s_savedViewAngles[1] = va[1]; s_savedViewAngles[2] = va[2];
                    auto* pw = EntityManager::Get().GetLocalPawn();
                    if (pw && pw->IsAlive()) {
                        float* ea = reinterpret_cast<float*>((uintptr_t)pw + Offsets::m_angEyeAngles);
                        s_savedEyeAngles[0] = ea[0]; s_savedEyeAngles[1] = ea[1]; s_savedEyeAngles[2] = ea[2];
                    }
                    s_anglesSaved = true;
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
        else {
            ::ShowCursor(FALSE); s_anglesSaved = false;
            if (oSDL_SetRelativeMouseMode)       oSDL_SetRelativeMouseMode(1);
            if (oSDL_SetWindowRelativeMouseMode) oSDL_SetWindowRelativeMouseMode(nullptr, 1);
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (Menu::IsOpen) Menu::Render();
    Visuals::Render();
    Menu::RenderOverlays();
    ImGui::Render();
    g_Context->OMSetRenderTargets(1, &g_RTV, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    if (Globals::misc_nightmode) {
        if (!g_NightModeActive) {
            HDC dc = GetDC(g_Window); GetDeviceGammaRamp(dc, g_OriginalGamma); ReleaseDC(g_Window, dc);
            g_NightModeActive = true;
        }
        HDC dc = GetDC(g_Window);
        WORD ramp[3][256];
        float br = Globals::misc_nightmode_brightness;
        for (int i = 0; i < 256; ++i) { WORD v = (WORD)min(65535.f, i * 256.f * br); ramp[0][i] = ramp[1][i] = ramp[2][i] = v; }
        SetDeviceGammaRamp(dc, ramp); ReleaseDC(g_Window, dc);
    }
    else if (g_NightModeActive) {
        HDC dc = GetDC(g_Window); SetDeviceGammaRamp(dc, g_OriginalGamma); ReleaseDC(g_Window, dc);
        g_NightModeActive = false;
    }

    return oPresent(swapChain, sync, flags);
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void Hooks::Setup()
{
    if (MH_Initialize() != MH_OK) return;

    NoFlash::Init();

    { // FrameStageNotify
        uintptr_t fsn = Memory::PatternScan("client.dll",
            "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC 40 48 8B F9 33 ED");
        if (fsn) MH_CreateHook(reinterpret_cast<void*>(fsn), &hkFrameStageNotify,
            reinterpret_cast<void**>(&oFrameStageNotify));
    }
    { // GetRawInputData
        void* fn = GetProcAddress(GetModuleHandleA("user32.dll"), "GetRawInputData");
        if (fn) MH_CreateHook(fn, &hkGetRawInputData, (void**)&oGetRawInputData);
    }
    { // SDL2 / SDL3
        HMODULE sdl = GetModuleHandleA("SDL2.dll");
        if (!sdl) sdl = GetModuleHandleA("SDL3.dll");
        if (sdl) {
            if (auto* fn = GetProcAddress(sdl, "SDL_SetRelativeMouseMode"))
                MH_CreateHook(fn, &hkSDL_SetRelativeMouseMode, (void**)&oSDL_SetRelativeMouseMode);
            if (auto* fn = GetProcAddress(sdl, "SDL_SetWindowRelativeMouseMode"))
                MH_CreateHook(fn, &hkSDL_SetWindowRelativeMouseMode, (void**)&oSDL_SetWindowRelativeMouseMode);
            if (auto* fn = GetProcAddress(sdl, "SDL_GetRelativeMouseState"))
                MH_CreateHook(fn, &hkSDL_GetRelativeMouseState, (void**)&oSDL_GetRelativeMouseState);
            if (auto* fn = GetProcAddress(sdl, "SDL_PollEvent"))
                MH_CreateHook(fn, &hkSDL_PollEvent, (void**)&oSDL_PollEvent);
        }
    }
    { // DrawModel (optional)
        HMODULE cm = GetModuleHandleA("client.dll");
        if (cm) {
            using CI_t = void* (*)(const char*, int*);
            auto CI = (CI_t)GetProcAddress(cm, "CreateInterface");
            if (CI) {
                void* mr = CI("VModelRender", nullptr);
                if (mr) MH_CreateHook((*reinterpret_cast<void***>(mr))[21], &hkDrawModel, (void**)&oDrawModel);
            }
        }
    }
    { // Present via dummy DX11 device
        WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"NecusDX";
        RegisterClassExW(&wc);
        HWND hw = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
        DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount = 1; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = hw; sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        IDXGISwapChain* sc = nullptr; ID3D11Device* dv = nullptr; ID3D11DeviceContext* cx = nullptr; D3D_FEATURE_LEVEL fl;
        if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &sd, &sc, &dv, &fl, &cx))) {
            void** vt = *reinterpret_cast<void***>(sc);
            MH_CreateHook(vt[8], &hkPresent, reinterpret_cast<void**>(&oPresent));
            MH_EnableHook(MH_ALL_HOOKS);
            sc->Release(); dv->Release(); cx->Release();
        }
        DestroyWindow(hw); UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }
}

// ─── Destroy ─────────────────────────────────────────────────────────────────
void Hooks::Destroy()
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    if (g_Window && oWndProc)
        SetWindowLongPtr(g_Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
    if (g_NightModeActive) { HDC dc = GetDC(g_Window); SetDeviceGammaRamp(dc, g_OriginalGamma); ReleaseDC(g_Window, dc); }
    if (!g_Init) return;

    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    if (g_RTV)     g_RTV->Release();
    if (g_Context) g_Context->Release();
    if (g_Device)  g_Device->Release();
    g_Init = false;
}
