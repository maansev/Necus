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
#include "../feature/misc/Movement.h"
#include "../feature/misc/AntiAim.h"
#include "../feature/misc/Viewmodel.h"
#include "../sdk/utils/Log.h"
#include <wingdi.h>
#include <cmath>

static bool g_NightModeActive = false;
static WORD g_OriginalGamma[3][256];

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#include <dxgi1_3.h>

ID3D11Device*            g_Device  = nullptr;
static ID3D11DeviceContext*    g_Context = nullptr;
static ID3D11RenderTargetView* g_RTV     = nullptr;
static HWND                    g_Window  = nullptr;
static bool                    g_Init    = false;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ─── Button offsets (cs2-dumper 2026-06-10) ──────────────────────────────────
// Button offsets confirmed cs2-dumper 2026-06-10
static constexpr ptrdiff_t BUTTON_JUMP      = 0x2065FA0;
static constexpr ptrdiff_t DW_VIEW_ANGLES   = 0x23568C8;
static constexpr uint32_t  BTN_PRESSED      = 0x10001u;
static constexpr uint32_t  BTN_RELEASED     = 0x100u;

// ─── BHop state ──────────────────────────────────────────────────────────────
static bool s_bhop_was_ground = false;
static bool s_bhop_roll_pass  = true;

// ─── Legit RCS: track last compensation so origVa reads are clean ────────────
static float s_rcsLastCompPitch = 0.f;
static float s_rcsLastCompYaw   = 0.f;


// ─── Cursor lock: view-angle freeze ──────────────────────────────────────────
static float s_frozenAngles[3] = {};
static bool  s_anglesValid     = false;
static bool  s_prevMenuOpen    = false;

// ─── Aspect ratio persistent state (file-scope so resize handler can reset) ──
static UINT s_arSrcW      = 0;
static UINT s_arBbW       = 0;
static UINT s_arBbH       = 0;
static bool s_arWasEnabled = false;

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
        } else if (type == 0x401u || type == 0x402u) {
            // SDL_MOUSEBUTTONDOWN / SDL_MOUSEBUTTONUP — swallow so game doesn't shoot
            *(uint32_t*)event = 0;
        } else if (type == 0x403u) {
            // SDL_MOUSEWHEEL — zero scroll so game camera doesn't zoom
            *(int32_t*)((uint8_t*)event + 16) = 0;
            *(int32_t*)((uint8_t*)event + 20) = 0;
        }
    }
    return result;
}

// NOTE: the old DrawModel hook ("VModelRender" vtable[21]) was removed —
// that interface is Source 1 only and does not exist in CS2's client.dll, so
// the hook never installed. Remove Legs/Shadows are done via memory writes in
// Visuals.cpp (GameSceneNode scale + m_flShadowStrength).

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
                raw->data.mouse.lLastX     = 0;
                raw->data.mouse.lLastY     = 0;
                raw->data.mouse.usButtonFlags = 0; // block clicks reaching game
                raw->data.mouse.usButtonData  = 0;
            } else if (Globals::aimbot_enabled && Globals::aimbot_lock_mouse && Aimbot::HasTarget()) {
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
        // Always feed ImGui first so it tracks mouse/keyboard state.
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return 1;
        // Swallow ALL mouse + raw input while the menu is open — the game never
        // sees it, so the camera can't move (SDL relative mode is also disabled
        // on menu open; no view-angle freezing hack is needed).
        if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) return 0;
        if (msg == WM_INPUT) return 0;
        // Swallow keyboard except the INSERT toggle (handled in hkPresent).
        if (msg == WM_KEYDOWN || msg == WM_KEYUP ||
            msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP ||
            msg == WM_CHAR || msg == WM_SYSCHAR) {
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

// ─── CCSGOInput::CreateMove — frame history silent aim ────────────────────────
// sig: 48 8B C4 4C 89 40 18 48 89 48 08 55 53 41 54 41 55  (client.dll)
// a1 = CCSGOInput* this, a2 = slot, a3 = context
// Before calling original: overwrite frame history pitch/yaw with aim angles.
// After original returns: restore originals so visual camera stays unchanged.
using CCSGOInputCreateMoveFn = double(__fastcall*)(__int64, unsigned int, __int64);
static CCSGOInputCreateMoveFn oCSGOInputCreateMove = nullptr;


double __fastcall hkCSGOInputCreateMove(__int64 a1, unsigned int a2, __int64 a3)
{
    static auto clampf = [](float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    };

    Log::Checkpoint("CM:enter");

    // ── Capture clean view angles BEFORE anything touches them ────────────────
    // dwViewAngles = raw mouse angles, NO punch included. Frame history entries
    // ALREADY have va+punch baked in by the engine before our hook fires.
    // We'll use these originals as the "no-punch" reference for RCS.
    uintptr_t client_va = Memory::GetModuleBase("client.dll");
    float origVaPitch = 0.f, origVaYaw = 0.f;
    float* vaPtr = nullptr;
    if (client_va) {
        vaPtr = reinterpret_cast<float*>(client_va + Offsets::dwViewAngles);
        origVaPitch = vaPtr[0];
        origVaYaw   = vaPtr[1];
    }

    // ── RCS ──────────────────────────────────────────────────────────────────
    // Read real aim punch from CCSPlayer_AimPunchServices::m_predictableBaseAngle
    // (confirmed offsets from cs2-dumper 2026-06-15 client_dll.hpp).
    // m_pAimPunchServices=0x1490 on pawn, m_predictableBaseAngle=0x50 relative to svc.
    bool rcsKeyed = Globals::rcs_enabled &&
        (Globals::rcs_key == 0 || (GetAsyncKeyState(Globals::rcs_key) & 0x8000));

    int       count = 0;
    uintptr_t arr   = 0;
    __try {
        count = *reinterpret_cast<int*>(a1 + Offsets::dwFrameHistoryCount);
        arr   = *reinterpret_cast<uintptr_t*>(a1 + Offsets::dwFrameHistoryArray);
    } __except (EXCEPTION_EXECUTE_HANDLER) { count = 0; arr = 0; }

    float punchPitch = 0.f, punchYaw = 0.f;
    if (rcsKeyed && client_va) {
        __try {
            uintptr_t localPawn = *reinterpret_cast<uintptr_t*>(client_va + Offsets::dwLocalPlayerPawn);
            if (localPawn) {
                uintptr_t svc = *reinterpret_cast<uintptr_t*>(localPawn + Offsets::m_pAimPunchServices);
                if (svc) {
                    punchPitch = *reinterpret_cast<float*>(svc + Offsets::m_aimPunchSvcBaseAngle);
                    punchYaw   = *reinterpret_cast<float*>(svc + Offsets::m_aimPunchSvcBaseAngle + 4);
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { punchPitch = 0.f; punchYaw = 0.f; }
    }

    // Hold the last non-zero punch to bridge the intermittent 0-reads from
    // m_predictableBaseAngle. Use fast decay (not hard 120ms hold) so punch
    // fades quickly after stop firing — prevents the over-compensation snap.
    // This is used ONLY for legit RCS; silent RCS reads raw punch directly.
    static float s_filteredLegitPitch = 0.f, s_filteredLegitYaw = 0.f;
    if (fabsf(punchPitch) > 0.05f || fabsf(punchYaw) > 0.05f) {
        s_filteredLegitPitch = punchPitch;
        s_filteredLegitYaw   = punchYaw;
    } else {
        s_filteredLegitPitch *= 0.5f;   // fast decay: ~3 ticks to negligible
        s_filteredLegitYaw   *= 0.5f;
    }

    bool doRcsSilent = rcsKeyed && Globals::rcs_silent;
    bool doRcsLegit  = rcsKeyed && !Globals::rcs_silent;
    bool rcsFiring   = (fabsf(punchPitch) > 0.05f || fabsf(punchYaw) > 0.05f);

    // Silent RCS: frame history already has punch baked in → subtract 2x raw punch.
    // Legit RCS: dwViewAngles is raw input, engine adds punch on top → subtract 1x.
    // compPitch/compYaw below are used for silent + debug log.
    float compPitch = clampf(punchPitch * 2.f * Globals::rcs_x_scale, -Globals::rcs_max_x, Globals::rcs_max_x);
    float compYaw   = clampf(punchYaw   * 2.f * Globals::rcs_y_scale, -Globals::rcs_max_y, Globals::rcs_max_y);

    if (Globals::chams_debug) {
        static int s_gdbg = 0;
        if ((s_gdbg++ % 120) == 0)
            Log::Write("RCS gate: rcs_enabled=%d rcs_key=0x%X keyed=%d silent=%d",
                       Globals::rcs_enabled ? 1 : 0, Globals::rcs_key,
                       rcsKeyed ? 1 : 0, Globals::rcs_silent ? 1 : 0);
        static int s_rdbg = 0;
        if (rcsKeyed && (s_rdbg++ % 15) == 0)
            Log::Write("RCS: punch=(%.3f,%.3f) comp=(%.3f,%.3f) firing=%d silent=%d fhCount=%d",
                       punchPitch, punchYaw, compPitch, compYaw,
                       rcsFiring ? 1 : 0, Globals::rcs_silent ? 1 : 0, count);
    }

    // Legit RCS: compensate view angle by tracking the DELTA of compensation,
    // so accumulated writes from previous ticks don't stack.
    // origVaPitch already has s_rcsLastCompPitch baked in from last tick → undo it first.
    if (doRcsLegit && vaPtr) {
        __try {
            // dwViewAngles is raw player input — engine adds punch on top for the camera.
            // camera = vaPtr + punch. For camera = truePitch: vaPtr = truePitch - punch.
            // Use filtered punch (fast decay) and 1x scale (not 2x which is for silent/frame-history).
            float legitCompPitch = fabsf(s_filteredLegitPitch) > 0.001f
                ? clampf(s_filteredLegitPitch * Globals::rcs_x_scale, -Globals::rcs_max_x, Globals::rcs_max_x) : 0.f;
            float legitCompYaw   = fabsf(s_filteredLegitYaw) > 0.001f
                ? clampf(s_filteredLegitYaw   * Globals::rcs_y_scale, -Globals::rcs_max_y, Globals::rcs_max_y) : 0.f;

            // Undo last tick's write so we're back to true player angle, then apply fresh comp.
            float truePitch = origVaPitch + s_rcsLastCompPitch;
            float trueYaw   = origVaYaw   + s_rcsLastCompYaw;

            float np = truePitch - legitCompPitch;
            float ny = trueYaw   - legitCompYaw;
            if (np >  89.f) np =  89.f; if (np < -89.f) np = -89.f;
            while (ny >  180.f) ny -= 360.f; while (ny < -180.f) ny += 360.f;
            vaPtr[0] = np; vaPtr[1] = ny;

            s_rcsLastCompPitch = legitCompPitch;
            s_rcsLastCompYaw   = legitCompYaw;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            s_rcsLastCompPitch = 0.f; s_rcsLastCompYaw = 0.f;
        }
    } else if (!doRcsLegit) {
        s_rcsLastCompPitch = 0.f;
        s_rcsLastCompYaw   = 0.f;
    }

    // ── No Spread: zero VData inaccuracy so every shot is 100% accurate ─────────
    // VData pointer at weapon+0x370 (confirmed from spread-circle code in Visuals.cpp).
    // We save values and restore them when disabled / weapon changes.
    {
        bool noSpreadActive = Globals::legit_no_spread || Globals::rage_no_spread;
        static struct {
            float spread[2]; float crouch[2]; float stand[2];
            float jump[2];   float land[2];   float fire[2];   float move[2];
        } s_ns = {};
        static bool      s_nsSaved   = false;
        static uintptr_t s_nsVData   = 0;

        __try {
            uintptr_t localPawn = client_va ? *reinterpret_cast<uintptr_t*>(client_va + Offsets::dwLocalPlayerPawn) : 0;
            uintptr_t weapSvc   = localPawn  ? *reinterpret_cast<uintptr_t*>(localPawn  + Offsets::m_pWeaponServices) : 0;
            uint32_t  wh        = weapSvc    ? *reinterpret_cast<uint32_t*> (weapSvc    + Offsets::m_hActiveWeapon)   : 0xFFFFFFFFu;
            uintptr_t weap      = 0;
            if (wh != 0xFFFFFFFFu && wh != 0 && client_va) {
                uintptr_t listPtr = *reinterpret_cast<uintptr_t*>(client_va + Offsets::dwEntityList);
                uintptr_t entry   = *reinterpret_cast<uintptr_t*>(listPtr + 8 * ((wh & 0x7FFF) >> 9) + 16);
                weap              = *reinterpret_cast<uintptr_t*>(entry   + 112 * (wh & 0x1FF));
            }
            uintptr_t vd = weap ? *reinterpret_cast<uintptr_t*>(weap + 0x370) : 0;

            if (!noSpreadActive) {
                // Restore when disabled
                if (s_nsSaved && vd == s_nsVData && vd) {
                    *reinterpret_cast<float*>(vd + Offsets::m_flSpread          + 0) = s_ns.spread[0];
                    *reinterpret_cast<float*>(vd + Offsets::m_flSpread          + 4) = s_ns.spread[1];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyCrouch+ 0) = s_ns.crouch[0];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyCrouch+ 4) = s_ns.crouch[1];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyStand + 0) = s_ns.stand[0];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyStand + 4) = s_ns.stand[1];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyJump  + 0) = s_ns.jump[0];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyJump  + 4) = s_ns.jump[1];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyLand  + 0) = s_ns.land[0];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyLand  + 4) = s_ns.land[1];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyFire  + 0) = s_ns.fire[0];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyFire  + 4) = s_ns.fire[1];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyMove  + 0) = s_ns.move[0];
                    *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyMove  + 4) = s_ns.move[1];
                }
                s_nsSaved = false; s_nsVData = 0;
            } else if (vd) {
                if (!s_nsSaved || s_nsVData != vd) {
                    // Save original VData values
                    s_ns.spread[0] = *reinterpret_cast<float*>(vd + Offsets::m_flSpread          + 0);
                    s_ns.spread[1] = *reinterpret_cast<float*>(vd + Offsets::m_flSpread          + 4);
                    s_ns.crouch[0] = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyCrouch+ 0);
                    s_ns.crouch[1] = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyCrouch+ 4);
                    s_ns.stand[0]  = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyStand + 0);
                    s_ns.stand[1]  = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyStand + 4);
                    s_ns.jump[0]   = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyJump  + 0);
                    s_ns.jump[1]   = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyJump  + 4);
                    s_ns.land[0]   = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyLand  + 0);
                    s_ns.land[1]   = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyLand  + 4);
                    s_ns.fire[0]   = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyFire  + 0);
                    s_ns.fire[1]   = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyFire  + 4);
                    s_ns.move[0]   = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyMove  + 0);
                    s_ns.move[1]   = *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyMove  + 4);
                    s_nsSaved = true; s_nsVData = vd;
                }
                // Zero all spread and inaccuracy values
                *reinterpret_cast<float*>(vd + Offsets::m_flSpread          + 0) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flSpread          + 4) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyCrouch+ 0) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyCrouch+ 4) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyStand + 0) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyStand + 4) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyJump  + 0) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyJump  + 4) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyLand  + 0) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyLand  + 4) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyFire  + 0) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyFire  + 4) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyMove  + 0) = 0.f;
                *reinterpret_cast<float*>(vd + Offsets::m_flInaccuracyMove  + 4) = 0.f;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { s_nsSaved = false; s_nsVData = 0; }
    }

    // ── Movement BEFORE original ──────────────────────────────────────────────
    __try {
        Movement::RunAutostrafer();
        Movement::RunQuickStop();
        Movement::RunQuickScope();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // ── Silent aim + silent RCS: modify frame history before original ─────────
    // Silent aim: inject target angles so the bullet goes to the target.
    // Silent RCS: write origVa (clean, no punch) to remove punch from the cmd.
    // Frame history after original is restored so rendering uses camera angles.
    bool doSilent = Aimbot::g_silentValid;
    float aimPitch = Aimbot::g_silentPitch;
    float aimYaw   = Aimbot::g_silentYaw;
    Aimbot::g_silentValid = false;

    // ── Anti-Aim ──────────────────────────────────────────────────────────────
    // Fake the networked angle via frame history. Skipped when silent aim fires
    // this tick (bullets must go to target) or when LMB is held (manual fire
    // would otherwise shoot the fake angle). Camera (dwViewAngles) untouched.
    float aaPitch = 0.f, aaYaw = 0.f;
    bool  doAntiAim = false;
    if (Globals::aa_enabled && !doSilent && client_va) {
        bool lmbHeld = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (!lmbHeld) {
            __try {
                uintptr_t aaPawn = *reinterpret_cast<uintptr_t*>(client_va + Offsets::dwLocalPlayerPawn);
                bool aliveOk = false, moveOk = false;
                if (aaPawn) {
                    aliveOk = *reinterpret_cast<int*>(aaPawn + Offsets::m_iHealth) > 0;
                    uint8_t mt = *reinterpret_cast<uint8_t*>(aaPawn + Offsets::m_MoveType);
                    moveOk = (mt != 8) && (mt != 9);   // not noclip / ladder
                }
                if (aliveOk && moveOk)
                    doAntiAim = AntiAim::Compute(origVaPitch, origVaYaw, aaPitch, aaYaw);
            } __except (EXCEPTION_EXECUTE_HANDLER) { doAntiAim = false; }
        }
    }

    struct FrameAngles { float pitch, yaw; };
    FrameAngles saved[64] = {};
    bool        wrote[64] = {};   // which entries we actually modified
    int safeCount = 0;

    bool needsFrameModify = doSilent || doRcsSilent || doAntiAim;
    if (needsFrameModify) {
        __try {
            // Validate the frame-history pointer/count before touching it. If the
            // dwFrameHistoryCount/Array offsets are stale after a CS2 update, `arr`
            // points at the wrong place and our angle writes scribble over real
            // pointers there → CS2's prediction later dereferences a float-as-pointer
            // and crashes (client.dll+0x96494B, AV read of 0x38D1B81F ≈ our pitch).
            bool arrOk = arr > 0x10000 && arr < 0x7FFFFFFFFFFFull
                         && count > 0 && count <= 64;
            if (arrOk) {
                // Per-entry sanity gate. The crash (client.dll+0x96494B, deterministic
                // AV read of 0x38D1B81F ≈ 0.0001) is a small float being dereferenced
                // as a pointer — i.e. our pitch landing in a pointer field. Validating
                // only entry[0] is not enough: if kFrameEntrySize is even slightly off,
                // entries i≥1 land mid-structure on top of a real pointer. So we now
                // validate EACH entry's current pitch/yaw and skip any that don't look
                // like plausible view angles. Only entries we write get restored.
                safeCount = count;
                for (int i = 0; i < safeCount; ++i) {
                    uintptr_t entry = arr + static_cast<uintptr_t>(Offsets::kFrameEntrySize) * i;
                    float* pp = reinterpret_cast<float*>(entry + Offsets::kFramePitchOffset);
                    float* py = reinterpret_cast<float*>(entry + Offsets::kFramePitchOffset + 4);
                    float curP = *pp;
                    float curY = *py;

                    // If this slot doesn't already hold a plausible angle, the layout
                    // is wrong here — never write, never restore. Prevents clobbering
                    // a pointer field with a float.
                    bool entryOk = (curP == curP) && (curY == curY)
                                   && curP >= -90.f  && curP <= 90.f
                                   && curY >= -180.f && curY <= 180.f;
                    if (!entryOk) {
                        static int s_warn = 0;
                        if ((s_warn++ % 240) == 0)
                            Log::Write("FrameHistory entry[%d] REJECTED: pitch=%.4f yaw=%.4f "
                                       "(stride/offset stale — skipping to avoid AV)", i, curP, curY);
                        continue;
                    }

                    saved[i] = { curP, curY };

                    // Silent aim sets the absolute target angle; silent RCS then
                    // subtracts the recoil compensation from whatever we send so
                    // the bullet lands where the (un-punched) crosshair points.
                    float np = doSilent ? aimPitch : (doAntiAim ? aaPitch : curP);
                    float ny = doSilent ? aimYaw   : (doAntiAim ? aaYaw   : curY);
                    if (doRcsSilent && rcsFiring) {
                        np -= compPitch; ny -= compYaw;
                    }
                    // Clamp to plausible range AND reject NaN before writing, so we
                    // never inject a value the engine could mistake for a pointer.
                    if ((np == np) && (ny == ny)
                        && np >= -90.f && np <= 90.f
                        && ny >= -180.f && ny <= 180.f) {
                        *pp = np; *py = ny;
                        wrote[i] = true;
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { safeCount = 0; }
    }

    double result = oCSGOInputCreateMove(a1, a2, a3);

    // ── Third person ─────────────────────────────────────────────────────────
    // Only write when third person is actually enabled. Writing 0 every single
    // tick is the one unconditional poke we make into the CCSGOInput object — if
    // m_bInThirdPerson (0x229) is stale after a CS2 update that write lands in the
    // wrong field and can corrupt the input object every tick. Gate it so a
    // disabled feature never touches game memory.
    if (Globals::misc_thirdperson) {
        __try {
            *reinterpret_cast<uint8_t*>(a1 + Offsets::m_bInThirdPerson) = 1;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // Restore frame history so rendering uses camera angles, not server angles.
    // Only restore entries we actually wrote — never touch a slot we skipped.
    if (safeCount > 0) {
        __try {
            for (int i = 0; i < safeCount; ++i) {
                if (!wrote[i]) continue;
                uintptr_t entry = arr + static_cast<uintptr_t>(Offsets::kFrameEntrySize) * i;
                *reinterpret_cast<float*>(entry + Offsets::kFramePitchOffset)     = saved[i].pitch;
                *reinterpret_cast<float*>(entry + Offsets::kFramePitchOffset + 4) = saved[i].yaw;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    Log::Checkpoint("CM:done");
    return result;
}

// ─── Anti-Flash: RenderFlashbangOverlay hook ─────────────────────────────────
// Intercepts the overlay render function — much more reliable than writing
// m_flFlashMaxAlpha which the engine overwrites between FSN stages.
using RenderFlashbangOverlayFn = void(__fastcall*)(void*, void*, void*, void*, void*);
static RenderFlashbangOverlayFn oRenderFlashbangOverlay = nullptr;
void __fastcall hkRenderFlashbangOverlay(void* a1, void* a2, void* a3, void* a4, void* a5) {
    if (Globals::misc_noflash) return;
    oRenderFlashbangOverlay(a1, a2, a3, a4, a5);
}

// ─── LevelInit hook ──────────────────────────────────────────────────────────
// Fires when a new map loads. Kept minimal — just log and call original.
// Previously called c_inventory_changer::invalidate_all() here but that
// triggered a C++ exception (0xE06D7363) inside oLevelInit which hit
// terminate() without VEH logging it (VEH skips C++ exceptions by design).
using LevelInitFn = void*(__fastcall*)(void*, const char*);
static LevelInitFn oLevelInit = nullptr;
void* __fastcall hkLevelInit(void* thisptr, const char* szNewMap) {
    Log::Write("LevelInit: map=%s", szNewMap ? szNewMap : "(null)");
    return oLevelInit(thisptr, szNewMap);
}

// ─── FrameStageNotify ────────────────────────────────────────────────────────
using FrameStageNotifyFn = void(__fastcall*)(void*, int);

// Safe wrapper — plain function (no C++ objects) so __try is legal (no C2712).
// oFrameStageNotify crashes on server join when ecx points to a partially
// destroyed view-system object. Catching it here prevents the game from dying.
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

    // Third person: write at the top of every FSN stage so we always win the
    // race against whichever stage the engine uses to reset m_bInThirdPerson.
    Visuals::WriteThirdPerson();

    // ── Stage 0 ───────────────────────────────────────────────────────────────
    if (stage == 0) {
        // Aimbot runs first (sets aim angles before movement processing)
        Log::Checkpoint("FSN0:Aimbot");
        Aimbot::Run(0);

        // Viewmodel offsets are sampled by the engine during prediction —
        // also write them at FRAME_START (and again at stage 5).
        Log::Checkpoint("FSN0:Viewmodel");
        __try { Viewmodel::Update(0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        Log::Checkpoint("FSN0:movement");

        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        __try {
            auto* pawn = EntityManager::Get().GetLocalPawn();
            if (!pawn || !pawn->IsAlive()) {
                s_bhop_was_ground = false;
                Log::Checkpoint("FSN0:deadpawn");
                // Auto-strafe / quick-stop now run in the CreateMove hook (the
                // only place movement writes survive). InfiniteDuck stays here —
                // it drives the duck button, which CreateMove reads.
                Movement::RunInfiniteDuck();
                return;
            }

            bool onGround = (*(int*)((uintptr_t)pawn + Offsets::m_fFlags) & 1) != 0;
            uint8_t moveType = *(uint8_t*)((uintptr_t)pawn + Offsets::m_MoveType);
            bool moveOk = (moveType != 9) && (moveType != 8);   // not LADDER / NOCLIP

            // ── BHop — classic "perfect" algorithm ───────────────────────────
            // if (holdingJump) { if (onGround) press; else release; }
            // Releasing in air lets the engine re-queue the next jump — much
            // smoother than the old double-flag state machine.
            {
                uintptr_t jumpAddr = client + BUTTON_JUMP;
                bool spaceHeld = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

                if (!Globals::misc_bhop || !moveOk) {
                    // Don't touch BUTTON_JUMP — let the game handle input normally
                    s_bhop_was_ground = false;
                } else if (!spaceHeld) {
                    *(uint32_t*)jumpAddr = BTN_RELEASED;
                    s_bhop_was_ground = false;
                } else {
                    // Legit mode: roll the chance once per landing (air→ground edge)
                    if (onGround && !s_bhop_was_ground)
                        s_bhop_roll_pass = (Globals::misc_bhop_mode == 0)
                            || ((rand() % 100) < Globals::misc_bhop_chance);

                    if (onGround && s_bhop_roll_pass)
                        *(uint32_t*)jumpAddr = BTN_PRESSED;
                    else
                        *(uint32_t*)jumpAddr = BTN_RELEASED;
                    s_bhop_was_ground = onGround;
                }
            }
            Log::Checkpoint("FSN0:bhop-done");

            // ── Auto Strafe / Quick Stop run in CreateMove (see hook) ──────────

            // ── Infinite Duck ─────────────────────────────────────────────────
            Movement::RunInfiniteDuck();
            Log::Checkpoint("FSN0:duck-done");

        } __except (EXCEPTION_EXECUTE_HANDLER) {
            s_bhop_was_ground = false;
        }
        // Marks the boundary: anything AFTER this checkpoint that crashes is the
        // engine's own movement/prediction (CGameMovement) running between
        // FSN_START and FSN_RENDER_START — not our stage-0 code.
        Log::Checkpoint("FSN0:done");
    }

    // ── Stage 5: FRAME_RENDER_START — right before the frame is drawn ─────────
    // Prediction has already run here, so m_aimPunchAngle is populated.
    // Run RCS now to read the valid punch and compensate view angles.
    if (stage == 5) {
        Log::Checkpoint("FSN5:RCS");
        __try { Aimbot::RunRCS_Stage5(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        Log::Checkpoint("FSN5:NoFlash");
        __try { NoFlash::Update(); }  __except (EXCEPTION_EXECUTE_HANDLER) {}
        Log::Checkpoint("FSN5:Viewmodel");
        __try { Viewmodel::Update(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        Log::Checkpoint("FSN5:OnRenderStart");
        __try { Visuals::OnRenderStart(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        Log::Checkpoint("FSN5:done");
    }

    // ── Stage 6: FRAME_RENDER_END — inventory changer ─────────────────────────
    // NOTE: the old "camera lock" (save/restore dwViewAngles while the menu is
    // open) was removed — it caused the view to snap back to centre every
    // frame. Blocking WM_INPUT/mouse in hkWndProc + disabling SDL relative
    // mouse mode fully stops camera movement while the menu is open.
    if (stage == 6) {
        Log::Checkpoint("FSN6:invchanger");
        __try { c_inventory_changer::get().run(6); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        Log::Checkpoint("FSN6:done");
    }

    // ── Stage 7: removals restore + silent aimbot restore ─────────────────────
    if (stage == 7) {
        Log::Checkpoint("FSN7:OnRenderEnd");
        __try { Visuals::OnRenderEnd(); } __except (EXCEPTION_EXECUTE_HANDLER) {}

        // Silent aimbot: восстановить визуальный угол (пули уже пошли по нужному направлению)
        Log::Checkpoint("FSN7:Aimbot");
        Aimbot::Run(7);
        Log::Checkpoint("FSN7:done");
    }
}

// ─── Present ─────────────────────────────────────────────────────────────────
// Recreate the render target view when the swapchain backbuffer is resized
// (CS2 calls ResizeBuffers during map loads / resolution changes, which
// invalidates any RTV created from the old backbuffer — using a stale RTV
// crashes the game in OMSetRenderTargets).
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
    Log::Checkpoint("Present:init");
    if (!g_Init) {
        if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_Device)))
            return oPresent(swapChain, sync, flags);

        g_Device->GetImmediateContext(&g_Context);
        DXGI_SWAP_CHAIN_DESC sd{};
        swapChain->GetDesc(&sd);
        g_Window = sd.OutputWindow;

        RebuildRTV(swapChain);
        Log::Write("Present: first init, window=0x%p rtv=0x%p", g_Window, g_RTV);

        oWndProc = (WNDPROC)SetWindowLongPtr(g_Window, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui_ImplWin32_Init(g_Window);
        ImGui_ImplDX11_Init(g_Device, g_Context);
        NecusLoadFontEmbedded(18.0f);
        NecusSetLogo(LoadNecusLogo(g_Device));

        // Init chams materials (DX11 device must be ready)
        bool chamsOk = Chams::Init();
        Log::Write("Chams::Init result=%d", (int)chamsOk);

        g_Init = true;
    }

    // Detect backbuffer resize (ResizeBuffers invalidates the old RTV).
    // Compare current backbuffer dimensions to what we had when g_RTV was created.
    {
        static UINT s_rtvW = 0, s_rtvH = 0;
        DXGI_SWAP_CHAIN_DESC sd{};
        if (SUCCEEDED(swapChain->GetDesc(&sd))) {
            UINT w = sd.BufferDesc.Width, h = sd.BufferDesc.Height;
            if (w != s_rtvW || h != s_rtvH || !g_RTV) {
                Log::Write("Present: backbuffer resize %ux%u -> %ux%u, rebuilding RTV",
                           s_rtvW, s_rtvH, w, h);
                RebuildRTV(swapChain);
                ImGui_ImplDX11_InvalidateDeviceObjects();
                s_rtvW = w; s_rtvH = h;
                // Reset AR statics so SetSourceSize gets re-applied at new size
                s_arSrcW = 0; s_arBbW = 0; s_arBbH = 0; s_arWasEnabled = false;
                Log::Write("Present: RTV rebuilt, rtv=0x%p", g_RTV);
            }
        }
    }

    Log::Checkpoint("Present:GetModuleBase");
    const uintptr_t client = Memory::GetModuleBase("client.dll");

    Log::Checkpoint("Present:CursorLock");
    // dwViewAngles is written here every frame before the game reads it again
    // in the next FrameStageNotify cycle. This is the most reliable cursor-lock
    // method in CS2 — bypasses any SDL/RawInput that leaks through the other hooks.
    __try {
        uintptr_t c0 = client;
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
            } else {
                s_anglesValid = false;
            }
            s_prevMenuOpen = Menu::IsOpen;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // Autostrafe runs in velocity mode (writes m_vecVelocity in CreateMove) and
    // never rotates the camera, so there is no view-angle write here anymore.

    // Third person distance: write to CPlayer_CameraServices (offset 0x214 = m_flDeathCamDistance).
    // Wrapped in SEH — if the offset is wrong nothing breaks, camera just uses engine default.
    if (Globals::misc_thirdperson && client) {
        __try {
            uintptr_t pawn = *reinterpret_cast<uintptr_t*>(client + Offsets::dwLocalPlayerPawn);
            if (pawn) {
                uintptr_t camSvc = *reinterpret_cast<uintptr_t*>(pawn + Offsets::m_pCameraServices);
                if (camSvc) {
                    *reinterpret_cast<float*>(camSvc + 0x214) = Globals::misc_thirdperson_dist;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    Log::Checkpoint("Present:EntityManager");
    __try { EntityManager::Get().Update(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    Log::Checkpoint("Present:UpdateRemovals");
    __try { Visuals::UpdateRemovals(); } __except (EXCEPTION_EXECUTE_HANDLER) {}

    Log::Checkpoint("Present:NoFlash");
    __try { NoFlash::Update(); } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // No Smoke — scan entity list for smoke projectiles and suppress them
    if (client) {
        __try {
            uintptr_t smList = *reinterpret_cast<uintptr_t*>(client + Offsets::dwEntityList);
            NoSmoke::Update(smList);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    Log::Checkpoint("Present:Binds+AutoAccept");
    __try { Menu::UpdateBinds(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    __try { Misc::RunAutoAccept(g_Window); } __except (EXCEPTION_EXECUTE_HANDLER) {}

    if (client)
        __try {
            memcpy(&Globals::ViewMatrix,
                   reinterpret_cast<void*>(client + Offsets::dwViewMatrix),
                   sizeof(Globals::ViewMatrix));
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

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
        } else {
            ::ShowCursor(FALSE);
            if (oSDL_SetRelativeMouseMode)       oSDL_SetRelativeMouseMode(1);
            if (oSDL_SetWindowRelativeMouseMode) oSDL_SetWindowRelativeMouseMode(nullptr, 1);
        }
    }

    // Aspect ratio: IDXGISwapChain2::SetSourceSize — DXGI takes only srcW×bbH
    // pixels from the back buffer and stretches them to fill the display.
    // We store srcW so ImGui uses it as logical screen width; that way the menu,
    // ESP and overlays are projected into [0,srcW] — exactly what DXGI shows.
    {
        bool needUpdate = Globals::misc_aspect_ratio_enabled || s_arWasEnabled;
        if (needUpdate) {
            IDXGISwapChain2* sc2 = nullptr;
            __try {
                if (SUCCEEDED(swapChain->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&sc2)) && sc2) {
                    DXGI_SWAP_CHAIN_DESC sd{};
                    swapChain->GetDesc(&sd);
                    s_arBbW = sd.BufferDesc.Width;
                    s_arBbH = sd.BufferDesc.Height;
                    s_arSrcW = s_arBbW;
                    if (Globals::misc_aspect_ratio_enabled) {
                        UINT srcW = (UINT)(s_arBbH * Globals::misc_aspect_ratio);
                        if (srcW < 16)      srcW = 16;
                        if (srcW > s_arBbW) srcW = s_arBbW;
                        s_arSrcW = srcW;
                    }
                    sc2->SetSourceSize(s_arSrcW, s_arBbH);
                    sc2->Release();
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) { if (sc2) sc2->Release(); }
        }
        s_arWasEnabled = Globals::misc_aspect_ratio_enabled;
    }

    Log::Checkpoint("Present:ImGuiNewFrame");
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    if (Globals::misc_aspect_ratio_enabled && s_arSrcW > 0 && s_arSrcW < s_arBbW)
        ImGui::GetIO().DisplaySize.x = (float)s_arSrcW;
    ImGui::NewFrame();
    Log::Checkpoint("Present:MenuRender");
    __try { if (Menu::IsOpen) Menu::Render(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    Log::Checkpoint("Present:VisualsRender");
    __try { Visuals::Render(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    Log::Checkpoint("Present:Overlays");
    __try { Menu::RenderOverlays(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    Log::Checkpoint("Present:ImGuiRender");
    ImGui::Render();
    if (g_RTV) {
        g_Context->OMSetRenderTargets(1, &g_RTV, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
    Log::Checkpoint("Present:postRender");

    // Nightmode — GDI SetDeviceGammaRamp is slow: only call it when brightness
    // actually changes, not every frame. GetDeviceGammaRamp only on first enable.
    if (Globals::misc_nightmode) {
        static float s_lastBr = -1.f;
        static WORD  s_cachedRamp[3][256] = {};
        if (!g_NightModeActive) {
            HDC dc = GetDC(g_Window);
            GetDeviceGammaRamp(dc, g_OriginalGamma);
            ReleaseDC(g_Window, dc);
            g_NightModeActive = true;
            s_lastBr = -1.f; // force ramp rebuild on first enable
        }
        float br = Globals::misc_nightmode_brightness;
        if (br != s_lastBr) {
            s_lastBr = br;
            for (int i = 0; i < 256; ++i) {
                WORD v = (WORD)min(65535.f, i * 256.f * br);
                s_cachedRamp[0][i] = s_cachedRamp[1][i] = s_cachedRamp[2][i] = v;
            }
            HDC dc = GetDC(g_Window);
            SetDeviceGammaRamp(dc, s_cachedRamp);
            ReleaseDC(g_Window, dc);
        }
    } else if (g_NightModeActive) {
        HDC dc = GetDC(g_Window);
        SetDeviceGammaRamp(dc, g_OriginalGamma);
        ReleaseDC(g_Window, dc);
        g_NightModeActive = false;
    }

    Log::Checkpoint("Present:oPresent");
    return oPresent(swapChain, sync, flags);
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void Hooks::Setup()
{
    Log::Init();
    Log::Write("Hooks::Setup start");

    if (MH_Initialize() != MH_OK) { Log::Write("MH_Initialize FAILED"); return; }

    NoFlash::Init();

    { // FrameStageNotify
        uintptr_t fsn = Memory::PatternScan("client.dll",
            "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC 40 48 8B F9 33 ED");
        Log::Write("FSN pattern: 0x%llX", (unsigned long long)fsn);
        if (fsn) {
            MH_STATUS s = MH_CreateHook(reinterpret_cast<void*>(fsn), &hkFrameStageNotify,
                               reinterpret_cast<void**>(&oFrameStageNotify));
            Log::Write("FSN hook: %s", MH_StatusToString(s));
        }
    }
    { // RenderFlashbangOverlay — suppress flashbang screen overlay when misc_noflash=true.
      // The hook is SAFE: when misc_noflash=false we always call the original.
      // If the pattern resolves to the wrong function, it will still call original → no crash.
        uintptr_t fn = Memory::PatternScan("client.dll",
            "48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 B4 24");
        Log::Write("RenderFlashbangOverlay pattern: 0x%llX", (unsigned long long)fn);
        if (fn) {
            MH_STATUS s = MH_CreateHook(reinterpret_cast<void*>(fn), &hkRenderFlashbangOverlay,
                               reinterpret_cast<void**>(&oRenderFlashbangOverlay));
            Log::Write("RFO hook: %s", MH_StatusToString(s));
        }
    }
    // LevelInit hook DISABLED — pattern was matching wrong function in client.dll,
    // causing corrupted game state on map load which led to FSN crash at client.dll+0x96494B.
    { // CCSGOInput::CreateMove — frame history silent aim
        uintptr_t fn = Memory::PatternScan("client.dll", Offsets::sig_CreateMove_client);
        Log::Write("CCSGOInput::CreateMove pattern: 0x%llX", (unsigned long long)fn);
        if (fn) {
            MH_STATUS s = MH_CreateHook(reinterpret_cast<void*>(fn),
                &hkCSGOInputCreateMove,
                reinterpret_cast<void**>(&oCSGOInputCreateMove));
            Log::Write("CCSGOInput::CreateMove hook: %s", MH_StatusToString(s));
        }
    }
    { // DrawObject (scenesystem.dll) — entity-level mesh draw hook for chams
        uintptr_t fn = Memory::PatternScan("scenesystem.dll",
            "48 8B C4 48 89 50 10 55 41 56");
        Log::Write("DrawObject pattern: 0x%llX", (unsigned long long)fn);
        if (fn) {
            if (MH_CreateHook(reinterpret_cast<void*>(fn),
                    &Chams::HookDrawArray,
                    reinterpret_cast<void**>(&Chams::oDrawArray)) == MH_OK)
                Log::Write("DrawObject hook installed");
            else
                Log::Write("DrawObject MH_CreateHook FAILED");
        }
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
    { // Present via dummy DX11 device
        WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"NecusDX";
        RegisterClassExW(&wc);
        HWND hw = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0,0,100,100,nullptr,nullptr,wc.hInstance,nullptr);
        DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount=1; sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow=hw; sd.SampleDesc.Count=1;
        sd.Windowed=TRUE; sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        IDXGISwapChain* sc=nullptr; ID3D11Device* dv=nullptr; ID3D11DeviceContext* cx=nullptr; D3D_FEATURE_LEVEL fl;
        if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,
                D3D11_SDK_VERSION,&sd,&sc,&dv,&fl,&cx))) {
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
    if (g_NightModeActive) { HDC dc=GetDC(g_Window); SetDeviceGammaRamp(dc,g_OriginalGamma); ReleaseDC(g_Window,dc); }
    if (!g_Init) return;

    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    if (g_RTV)     g_RTV->Release();
    if (g_Context) g_Context->Release();
    if (g_Device)  g_Device->Release();
    g_Init = false;
}
