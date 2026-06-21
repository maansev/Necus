#include "Movement.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/memory/Offsets.h"
#include "../../sdk/memory/PatternScan.h"
#include "../../sdk/utils/Log.h"
#include <Windows.h>
#include <cmath>

// Button addresses from cs2-dumper 2026-06-15
static constexpr ptrdiff_t BTN_FORWARD = 0x2065CD0;
static constexpr ptrdiff_t BTN_BACK    = 0x2065D60;
static constexpr ptrdiff_t BTN_LEFT    = 0x2065DF0;
static constexpr ptrdiff_t BTN_RIGHT   = 0x2065E80;
static constexpr ptrdiff_t BTN_DUCK    = 0x2066030;
static constexpr ptrdiff_t DW_VA       = 0x23568C8;
static constexpr uint32_t  BTN_PRESSED  = 0x10001u;
static constexpr uint32_t  BTN_RELEASED = 0x100u;

static constexpr float DEG2RAD = 3.14159265f / 180.0f;
static constexpr float RAD2DEG = 180.0f / 3.14159265f;

static bool  s_duck_forced     = false;
static bool  s_strafeBaseValid = false;
static float s_strafeBaseYaw   = 0.f;
static int   s_strafeDir       = 1;
static bool  s_strafeWasInAir  = false;
static float s_strafeTurned    = 0.f;   // degrees turned in current direction

static bool  s_qsActive        = false; // quickstop is actively counter-pressing

bool  Movement::g_strafeActive    = false;
float Movement::g_strafeTargetYaw = 0.f;

static float NormAngle(float a) {
    while (a >  180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}

// Auto-strafe: called from CreateMove hook (before original).
// CS2 yaw convention: increasing yaw = turning LEFT (counterclockwise).
// Air-strafe physics: turn LEFT + press A builds speed; turn RIGHT + press D builds speed.
// dir=+1 → yaw increases → turning LEFT → press A (BTN_LEFT)
// dir=-1 → yaw decreases → turning RIGHT → press D (BTN_RIGHT)
void Movement::RunAutostrafer()
{
    if (!Globals::misc_autostrafer) {
        if (g_strafeActive) {
            uintptr_t client = Memory::GetModuleBase("client.dll");
            if (client) {
                *(uint32_t*)(client + BTN_LEFT)    = BTN_RELEASED;
                *(uint32_t*)(client + BTN_RIGHT)   = BTN_RELEASED;
            }
        }
        g_strafeActive    = false;
        s_strafeBaseValid = false;
        s_strafeWasInAir  = false;
        s_strafeTurned    = 0.f;
        return;
    }

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) { g_strafeActive = false; return; }

    auto* pawnObj = EntityManager::Get().GetLocalPawn();
    if (!pawnObj || !pawnObj->IsAlive()) {
        g_strafeActive = false; s_strafeBaseValid = false; s_strafeWasInAir = false; return;
    }
    uintptr_t pawn = reinterpret_cast<uintptr_t>(pawnObj);

    uint8_t moveType = *(uint8_t*)(pawn + Offsets::m_MoveType);
    if (moveType == 9 || moveType == 8) {
        g_strafeActive = false; s_strafeBaseValid = false; s_strafeWasInAir = false; return;
    }

    int  flags    = *(int*)(pawn + Offsets::m_fFlags);
    bool onGround = (flags & 1) != 0;

    if (onGround) {
        if (s_strafeWasInAir) {
            *(uint32_t*)(client + BTN_LEFT)  = BTN_RELEASED;
            *(uint32_t*)(client + BTN_RIGHT) = BTN_RELEASED;
        }
        g_strafeActive    = false;
        s_strafeBaseValid = false;
        s_strafeWasInAir  = false;
        s_strafeTurned    = 0.f;
        return;
    }
    s_strafeWasInAir = true;

    // Release W/S — forward/back input hurts air-strafe acceleration
    *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
    *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED;

    float* vel  = reinterpret_cast<float*>(pawn + Offsets::m_vecVelocity);
    float  vx = vel[0], vy = vel[1];
    float  speed = sqrtf(vx * vx + vy * vy);

    float* va    = reinterpret_cast<float*>(client + DW_VA);
    float  curYaw = va[1];

    // On first air tick, orient base around velocity direction if moving
    if (!s_strafeBaseValid) {
        s_strafeBaseYaw   = (speed > 20.f) ? NormAngle(RAD2DEG * atan2f(vy, vx)) : curYaw;
        s_strafeBaseValid = true;
        s_strafeDir       = 1;
        s_strafeTurned    = 0.f;
    }

    const float kAmplitude = 25.0f;  // degrees per half-cycle
    const float kStep      = 5.0f;   // degrees per CreateMove tick (5 * 64Hz = 320°/s)

    s_strafeTurned += kStep;
    if (s_strafeTurned >= kAmplitude) {
        s_strafeDir    = -s_strafeDir;
        s_strafeTurned = 0.f;
    }

    g_strafeTargetYaw = NormAngle(curYaw + s_strafeDir * kStep);
    g_strafeActive    = true;

    if (s_strafeDir > 0) {
        // Yaw increasing = turning LEFT → press A
        *(uint32_t*)(client + BTN_LEFT)  = BTN_PRESSED;
        *(uint32_t*)(client + BTN_RIGHT) = BTN_RELEASED;
    } else {
        // Yaw decreasing = turning RIGHT → press D
        *(uint32_t*)(client + BTN_RIGHT) = BTN_PRESSED;
        *(uint32_t*)(client + BTN_LEFT)  = BTN_RELEASED;
    }

    static int s_dbg = 0;
    if ((s_dbg++ % 30) == 0)
        Log::Write("STRAFE: speed=%.1f dir=%d turned=%.1f tgtYaw=%.1f",
                   speed, s_strafeDir, s_strafeTurned, g_strafeTargetYaw);
}

// Quick-stop: hold counter-movement buttons until speed drops below threshold.
// Recalculates counter direction each tick as velocity rotates during braking.
void Movement::RunQuickStop()
{
    uintptr_t client = Memory::GetModuleBase("client.dll");

    if (!Globals::misc_quickstop) {
        if (s_qsActive && client) {
            *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
            *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED;
            *(uint32_t*)(client + BTN_LEFT)    = BTN_RELEASED;
            *(uint32_t*)(client + BTN_RIGHT)   = BTN_RELEASED;
            s_qsActive = false;
        }
        return;
    }

    if (!client) return;

    auto* pawnObj = EntityManager::Get().GetLocalPawn();
    if (!pawnObj || !pawnObj->IsAlive()) { s_qsActive = false; return; }
    uintptr_t pawn = reinterpret_cast<uintptr_t>(pawnObj);

    uint8_t moveType = *(uint8_t*)(pawn + Offsets::m_MoveType);
    int     flags    = *(int*)(pawn + Offsets::m_fFlags);
    bool    onGround = (flags & 1) != 0;
    if (!onGround || moveType == 9 || moveType == 8) { s_qsActive = false; return; }

    // If player presses any WASD: release our buttons and let them control
    bool wasdHeld = (GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState('S') & 0x8000) ||
                    (GetAsyncKeyState('A') & 0x8000) || (GetAsyncKeyState('D') & 0x8000);
    if (wasdHeld) {
        if (s_qsActive) {
            *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
            *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED;
            *(uint32_t*)(client + BTN_LEFT)    = BTN_RELEASED;
            *(uint32_t*)(client + BTN_RIGHT)   = BTN_RELEASED;
            s_qsActive = false;
        }
        return;
    }

    float* vel  = reinterpret_cast<float*>(pawn + Offsets::m_vecVelocity);
    float  vx = vel[0], vy = vel[1];
    float  speed = sqrtf(vx * vx + vy * vy);

    if (speed < 8.f) {
        if (s_qsActive) {
            *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
            *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED;
            *(uint32_t*)(client + BTN_LEFT)    = BTN_RELEASED;
            *(uint32_t*)(client + BTN_RIGHT)   = BTN_RELEASED;
            s_qsActive = false;
        }
        return;
    }

    float* va      = reinterpret_cast<float*>(client + DW_VA);
    float  worldYaw = RAD2DEG * atan2f(vy, vx);
    float  localRad = NormAngle(worldYaw - va[1]) * DEG2RAD;

    float fwdVel  = cosf(localRad);
    float sideVel = sinf(localRad);

    // Counter forward/back component
    if (fabsf(fwdVel) > 0.1f) {
        if (fwdVel > 0.f) {
            *(uint32_t*)(client + BTN_BACK)    = BTN_PRESSED;
            *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
        } else {
            *(uint32_t*)(client + BTN_FORWARD) = BTN_PRESSED;
            *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED;
        }
    } else {
        *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
        *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED;
    }

    // Counter side component
    // sideVel > 0 = velocity is to the LEFT of facing → counter with D (BTN_RIGHT)
    // sideVel < 0 = velocity is to the RIGHT of facing → counter with A (BTN_LEFT)
    if (fabsf(sideVel) > 0.1f) {
        if (sideVel > 0.f) {
            *(uint32_t*)(client + BTN_RIGHT) = BTN_PRESSED;
            *(uint32_t*)(client + BTN_LEFT)  = BTN_RELEASED;
        } else {
            *(uint32_t*)(client + BTN_LEFT)  = BTN_PRESSED;
            *(uint32_t*)(client + BTN_RIGHT) = BTN_RELEASED;
        }
    } else {
        *(uint32_t*)(client + BTN_LEFT)  = BTN_RELEASED;
        *(uint32_t*)(client + BTN_RIGHT) = BTN_RELEASED;
    }

    s_qsActive = true;

    static int s_dbg = 0;
    if ((s_dbg++ % 10) == 0)
        Log::Write("QSTOP: speed=%.1f fwdV=%.2f sideV=%.2f", speed, fwdVel, sideVel);
}

void Movement::RunInfiniteDuck()
{
    if (!Globals::misc_infinite_duck) {
        if (s_duck_forced) {
            uintptr_t client = Memory::GetModuleBase("client.dll");
            if (client) *(uint32_t*)(client + BTN_DUCK) = BTN_RELEASED;
            s_duck_forced = false;
        }
        return;
    }

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;

    auto* pawn = EntityManager::Get().GetLocalPawn();
    if (!pawn || !pawn->IsAlive()) return;

    bool duckHeld = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;
    if (duckHeld) {
        *(uint32_t*)(client + BTN_DUCK) = BTN_PRESSED;
        s_duck_forced = true;
    } else if (s_duck_forced) {
        *(uint32_t*)(client + BTN_DUCK) = BTN_RELEASED;
        s_duck_forced = false;
    }

    *(float*)((uintptr_t)pawn + Offsets::m_flDuckSpeed) = 8.0f;
}
