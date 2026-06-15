#include "Movement.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/memory/Offsets.h"
#include "../../sdk/memory/PatternScan.h"
#include <Windows.h>
#include <cmath>
#include <algorithm>

static constexpr ptrdiff_t BTN_FORWARD   = 0x2065CD0;
static constexpr ptrdiff_t BTN_MOVELEFT  = 0x2065DF0;
static constexpr ptrdiff_t BTN_MOVERIGHT = 0x2065E80;
static constexpr ptrdiff_t BTN_DUCK      = 0x2066030;
static constexpr ptrdiff_t DW_VA         = 0x23568C8;
static constexpr uint32_t  BTN_PRESSED   = 0x10001u;
static constexpr uint32_t  BTN_RELEASED  = 0x100u;

static constexpr float AIR_ACCEL = 15.0f;   // atan2(AIR_ACCEL, speed) = ideal strafe angle
static constexpr float DEG2RAD   = 3.14159265f / 180.0f;
static constexpr float RAD2DEG   = 180.0f / 3.14159265f;

static bool  s_duck_forced  = false;
static int   s_strafeState  = 0;
static float s_prevYaw      = 0.f;
static bool  s_prevYawValid = false;

static float NormAngle(float a) {
    while (a >  180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}

void Movement::RunAutostrafer()
{
    uintptr_t client = Memory::GetModuleBase("client.dll");

    if (!Globals::misc_autostrafer) {
        if (client) {
            if (s_strafeState == 1) *(uint32_t*)(client + BTN_MOVELEFT)  = BTN_RELEASED;
            if (s_strafeState == 2) *(uint32_t*)(client + BTN_MOVERIGHT) = BTN_RELEASED;
        }
        s_strafeState  = 0;
        s_prevYawValid = false;
        return;
    }

    if (!client) return;

    auto* pawn = EntityManager::Get().GetLocalPawn();
    if (!pawn || !pawn->IsAlive()) {
        s_strafeState  = 0;
        s_prevYawValid = false;
        return;
    }

    uint8_t moveType = *(uint8_t*)((uintptr_t)pawn + Offsets::m_MoveType);
    if (moveType == 9 || moveType == 8) { s_strafeState = 0; return; }

    int  flags    = *(int*)((uintptr_t)pawn + Offsets::m_fFlags);
    bool onGround = (flags & 1) != 0;

    uintptr_t btnL = client + BTN_MOVELEFT;
    uintptr_t btnR = client + BTN_MOVERIGHT;

    if (onGround) {
        if (s_strafeState == 1) *(uint32_t*)btnL = BTN_RELEASED;
        if (s_strafeState == 2) *(uint32_t*)btnR = BTN_RELEASED;
        s_strafeState  = 0;
        s_prevYawValid = false;
        return;
    }

    // ── In air ──────────────────────────────────────────────────────────────────
    float* va      = reinterpret_cast<float*>(client + DW_VA);
    float  curYaw  = va[1];

    float* vel      = reinterpret_cast<float*>((uintptr_t)pawn + Offsets::m_vecVelocity);
    float  vx = vel[0], vy = vel[1];
    float  speed    = sqrtf(vx * vx + vy * vy);

    if (speed < 15.0f) {
        s_prevYawValid = false;
        return;
    }

    // ── Yaw delta (mouse movement) ─────────────────────────────────────────────
    float yawDelta = 0.f;
    if (s_prevYawValid) {
        yawDelta = NormAngle(curYaw - s_prevYaw);
    }
    s_prevYaw      = curYaw;
    s_prevYawValid = true;

    // ── Determine strafe direction ─────────────────────────────────────────────
    float sideMove = 0.f;

    if (yawDelta > 0.05f) {
        // Мышь влево → жмём A
        sideMove = -1.f;
    } else if (yawDelta < -0.05f) {
        // Мышь вправо → жмём D
        sideMove = 1.f;
    } else {
        // Мышь не двигается — авто-расчёт через угол скорости
        float velYaw   = RAD2DEG * atan2f(vy, vx);
        float velDelta = NormAngle(curYaw - velYaw);

        float idealAngle = RAD2DEG * atan2f(AIR_ACCEL, speed);
        idealAngle = std::clamp(idealAngle, 0.f, 90.f);

        if (velDelta > 0.f)
            sideMove = -1.f;
        else
            sideMove = 1.f;
    }

    // ── Write normalized sidemove to MovementServices ─────────────────────────
    uintptr_t movSvc = *(uintptr_t*)((uintptr_t)pawn + Offsets::m_pMovementServices);
    if (movSvc) {
        *(float*)(movSvc + Offsets::m_flCmdLeftMove)    = sideMove;
        *(float*)(movSvc + Offsets::m_flCmdForwardMove) = 0.f;

        if (Globals::misc_autostrafer_subtick) {
            float* sw = reinterpret_cast<float*>(movSvc + Offsets::m_arrForceSubtickMoveWhen);
            sw[0] = 0.f; sw[1] = 0.f; sw[2] = 0.f; sw[3] = 0.f;
        }
    }

    // ── Force corresponding button ─────────────────────────────────────────────
    if (sideMove > 0.f) {
        *(uint32_t*)btnR = BTN_PRESSED;
        *(uint32_t*)btnL = BTN_RELEASED;
        s_strafeState = 2;
    } else if (sideMove < 0.f) {
        *(uint32_t*)btnL = BTN_PRESSED;
        *(uint32_t*)btnR = BTN_RELEASED;
        s_strafeState = 1;
    }

    // Release W in air to remove forward-friction penalty
    if (Globals::misc_autostrafer_subtick)
        *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
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
