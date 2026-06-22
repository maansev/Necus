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

// Auto-strafe (velocity mode): accelerates toward look direction without rotating
// the camera. Directly writes m_vecVelocity each tick so camera stays frozen.
// CS2 yaw convention: yaw=0 → +X, yaw=90 → +Y (counterclockwise).
void Movement::RunAutostrafer()
{
    if (!Globals::misc_autostrafer) {
        if (g_strafeActive) {
            uintptr_t client = Memory::GetModuleBase("client.dll");
            if (client) __try {
                *(uint32_t*)(client + BTN_LEFT)    = BTN_RELEASED;
                *(uint32_t*)(client + BTN_RIGHT)   = BTN_RELEASED;
                *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
                *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED;
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        g_strafeActive    = false;
        s_strafeBaseValid = false;
        s_strafeWasInAir  = false;
        s_strafeTurned    = 0.f;
        return;
    }

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) { g_strafeActive = false; return; }

    uintptr_t pawn = 0;
    // Re-derive local pawn fresh each call to avoid stale-pointer AV when the
    // entity is freed between the EntityManager update (Present thread) and our
    // FSN call. EntityManager::GetLocalPawn() returns a raw pointer with no
    // lifetime guarantee across the thread boundary.
    __try {
        uintptr_t localPawnAddr = *reinterpret_cast<uintptr_t*>(client + Offsets::dwLocalPlayerPawn);
        if (localPawnAddr > 0x10000 && localPawnAddr < 0x7FFFFFFFFFFFull) {
            int32_t hp = *reinterpret_cast<int32_t*>(localPawnAddr + Offsets::m_iHealth);
            if (hp > 0) pawn = localPawnAddr;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    if (!pawn) {
        g_strafeActive = false; s_strafeBaseValid = false; s_strafeWasInAir = false; return;
    }

    __try {
        uint8_t moveType = *(uint8_t*)(pawn + Offsets::m_MoveType);
        if (moveType == 9 || moveType == 8) {
            g_strafeActive = false; s_strafeBaseValid = false; s_strafeWasInAir = false; return;
        }

        int  flags    = *(int*)(pawn + Offsets::m_fFlags);
        bool onGround = (flags & 1) != 0;

        if (onGround) {
            if (s_strafeWasInAir) {
                *(uint32_t*)(client + BTN_LEFT)    = BTN_RELEASED;
                *(uint32_t*)(client + BTN_RIGHT)   = BTN_RELEASED;
                *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
                *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED;
            }
            g_strafeActive    = false;
            s_strafeBaseValid = false;
            s_strafeWasInAir  = false;
            s_strafeTurned    = 0.f;
            return;
        }
        s_strafeWasInAir = true;

        bool spaceHeld = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        if (!spaceHeld) { g_strafeActive = false; return; }
        g_strafeActive    = true;
        g_strafeTargetYaw = 0.f;

        float* va   = reinterpret_cast<float*>(client + DW_VA);
        float  yaw  = va[1] * DEG2RAD;
        float  dirX = cosf(yaw);
        float  dirY = sinf(yaw);
        if (!(dirX == dirX) || !(dirY == dirY)) { g_strafeActive = false; return; }

        float* vel   = reinterpret_cast<float*>(pawn + Offsets::m_vecVelocity);
        float  vx    = vel[0], vy = vel[1];
        float  speed = sqrtf(vx * vx + vy * vy);
        // Sanity: if velocity is nonsense, don't touch it.
        if (!(speed == speed) || speed > 10000.f) { g_strafeActive = false; return; }

        const float kMaxSpeed = 285.f;
        const float kAccel    = 60.f;

        if (speed < kMaxSpeed) {
            float newVx = vx + dirX * kAccel;
            float newVy = vy + dirY * kAccel;
            float newSpeed = sqrtf(newVx * newVx + newVy * newVy);
            if (newSpeed > kMaxSpeed) { float sc = kMaxSpeed / newSpeed; newVx *= sc; newVy *= sc; }
            vel[0] = newVx; vel[1] = newVy;
        } else {
            float newVx = vx + dirX * kAccel * 0.3f;
            float newVy = vy + dirY * kAccel * 0.3f;
            float newSpeed = sqrtf(newVx * newVx + newVy * newVy);
            if (newSpeed > 0.f) { float sc = speed / newSpeed; newVx *= sc; newVy *= sc; }
            vel[0] = newVx; vel[1] = newVy;
        }

        static int s_dbg = 0;
        if ((s_dbg++ % 30) == 0)
            Log::Write("STRAFE: speed=%.1f dir=(%.2f,%.2f)", sqrtf(vel[0]*vel[0]+vel[1]*vel[1]), dirX, dirY);
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_strafeActive = false; }
}

// Apply counter-strafe buttons to kill horizontal velocity.
// Returns true when actively braking.
static bool ApplyCounterStrafe(uintptr_t client, uintptr_t pawn)
{
    float* vel   = reinterpret_cast<float*>(pawn + Offsets::m_vecVelocity);
    float* va    = reinterpret_cast<float*>(client + DW_VA);
    float  vx    = vel[0], vy = vel[1];
    float  speed = sqrtf(vx * vx + vy * vy);
    if (!(speed == speed) || speed > 10000.f) return false;

    if (speed < 4.f) {
        // Already stopped — release everything and exit.
        *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
        *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED;
        *(uint32_t*)(client + BTN_LEFT)    = BTN_RELEASED;
        *(uint32_t*)(client + BTN_RIGHT)   = BTN_RELEASED;
        return false;
    }

    float worldYaw = RAD2DEG * atan2f(vy, vx);
    float localRad = NormAngle(worldYaw - va[1]) * DEG2RAD;
    float fwdVel   = cosf(localRad);
    float sideVel  = sinf(localRad);

    if (fabsf(fwdVel) > 0.08f) {
        if (fwdVel > 0.f) { *(uint32_t*)(client + BTN_BACK)    = BTN_PRESSED;  *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED; }
        else               { *(uint32_t*)(client + BTN_FORWARD) = BTN_PRESSED;  *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED; }
    } else {
        *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
        *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED;
    }
    if (fabsf(sideVel) > 0.08f) {
        if (sideVel > 0.f) { *(uint32_t*)(client + BTN_RIGHT) = BTN_PRESSED;  *(uint32_t*)(client + BTN_LEFT)  = BTN_RELEASED; }
        else                { *(uint32_t*)(client + BTN_LEFT)  = BTN_PRESSED;  *(uint32_t*)(client + BTN_RIGHT) = BTN_RELEASED; }
    } else {
        *(uint32_t*)(client + BTN_LEFT)  = BTN_RELEASED;
        *(uint32_t*)(client + BTN_RIGHT) = BTN_RELEASED;
    }
    return true;
}

static void ReleaseMovement(uintptr_t client) {
    *(uint32_t*)(client + BTN_FORWARD) = BTN_RELEASED;
    *(uint32_t*)(client + BTN_BACK)    = BTN_RELEASED;
    *(uint32_t*)(client + BTN_LEFT)    = BTN_RELEASED;
    *(uint32_t*)(client + BTN_RIGHT)   = BTN_RELEASED;
}

// Quick-stop: three modes:
//   0 = In Air     — zero horizontal velocity at jump peak (works in air, fires counter-keys)
//   1 = On Land    — counter-strafe only when on the ground and WASD released (default)
//   2 = Between Shots — counter-strafe whenever rage has a target, overrides WASD
void Movement::RunQuickStop()
{
    uintptr_t client = Memory::GetModuleBase("client.dll");

    if (!Globals::misc_quickstop) {
        if (s_qsActive && client) __try { ReleaseMovement(client); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        s_qsActive = false;
        return;
    }
    if (!client) return;

    uintptr_t pawn = 0;
    __try {
        uintptr_t addr = *reinterpret_cast<uintptr_t*>(client + Offsets::dwLocalPlayerPawn);
        if (addr > 0x10000 && addr < 0x7FFFFFFFFFFFull) {
            int32_t hp = *reinterpret_cast<int32_t*>(addr + Offsets::m_iHealth);
            if (hp > 0) pawn = addr;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (!pawn) { s_qsActive = false; return; }

    __try {
        uint8_t moveType = *(uint8_t*)(pawn + Offsets::m_MoveType);
        if (moveType == 9 || moveType == 8) {   // noclip / ladder
            if (s_qsActive) { ReleaseMovement(client); s_qsActive = false; }
            return;
        }

        int  flags    = *(int*)(pawn + Offsets::m_fFlags);
        bool onGround = (flags & 1) != 0;
        bool wasdHeld = (GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState('S') & 0x8000) ||
                        (GetAsyncKeyState('A') & 0x8000) || (GetAsyncKeyState('D') & 0x8000);

        int mode = Globals::misc_quickstop_mode;

        if (mode == 0) {
            // In Air: zero horiz velocity at peak of jump (when in air + space held)
            if (!onGround && (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
                // Read velocity directly; write counter-keys to let the game engine
                // apply friction in the right direction.
                s_qsActive = ApplyCounterStrafe(client, pawn);
            } else {
                if (s_qsActive) { ReleaseMovement(client); s_qsActive = false; }
            }
        } else if (mode == 2) {
            // Between Shots: always counter-strafe when aimbot has a target lined up,
            // even while WASD is held (bot overrides movement to guarantee accuracy).
            // We gate on whether rage/silent aim is active this tick via g_strafeActive
            // as a proxy — or simply always strafe; the rage hit-chance gate will suppress
            // firing if we're still moving too fast.
            if (onGround) {
                s_qsActive = ApplyCounterStrafe(client, pawn);
            } else {
                if (s_qsActive) { ReleaseMovement(client); s_qsActive = false; }
            }
        } else {
            // Mode 1 (On Land, default): counter-strafe only when grounded AND no WASD
            if (!onGround) {
                if (s_qsActive) { ReleaseMovement(client); s_qsActive = false; }
                return;
            }
            if (wasdHeld) {
                if (s_qsActive) { ReleaseMovement(client); s_qsActive = false; }
                return;
            }
            s_qsActive = ApplyCounterStrafe(client, pawn);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { s_qsActive = false; }
}

// Quick Scope: when using a scoped weapon (AWP, SSG08, AUG, SG553, SCAR-20, G3SG1)
// and aimbot_auto_scope is enabled, send a simulated right-click to zoom in.
// Rate-limited to avoid toggling scope every tick; re-scopes after unscopng too.
void Movement::RunQuickScope()
{
    if (!Globals::aimbot_auto_scope) return;

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;

    uintptr_t pawn = 0;
    __try {
        uintptr_t addr = *reinterpret_cast<uintptr_t*>(client + Offsets::dwLocalPlayerPawn);
        if (addr > 0x10000 && addr < 0x7FFFFFFFFFFFull) {
            int32_t hp = *reinterpret_cast<int32_t*>(addr + Offsets::m_iHealth);
            if (hp > 0) pawn = addr;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (!pawn) return;

    __try {
        uintptr_t ws = *reinterpret_cast<uintptr_t*>(pawn + Offsets::m_pWeaponServices);
        if (!ws) return;
        uint32_t wh = *reinterpret_cast<uint32_t*>(ws + Offsets::m_hActiveWeapon);
        if (!wh || wh == 0xFFFFFFFFu) return;

        // Use entity list to resolve weapon entity and read def_index
        uintptr_t listPtr = *reinterpret_cast<uintptr_t*>(client + Offsets::dwEntityList);
        uintptr_t entry   = *reinterpret_cast<uintptr_t*>(listPtr + 8 * ((wh & 0x7FFF) >> 9) + 16);
        uintptr_t weap    = *reinterpret_cast<uintptr_t*>(entry + 112 * (wh & 0x1FF));
        if (!weap) return;

        uint16_t def = *reinterpret_cast<uint16_t*>(
            weap + Offsets::m_AttributeManager + 0x50 + Offsets::m_iItemDefinitionIndex);

        // Scoped rifles by def_index
        bool scopedWeapon = (def == 9  ||   // AWP
                             def == 40 ||   // SSG 08
                             def == 39 ||   // SG 553
                             def == 8  ||   // AUG
                             def == 38 ||   // SCAR-20
                             def == 11);    // G3SG1
        if (!scopedWeapon) return;

        bool isScoped = *reinterpret_cast<bool*>(pawn + Offsets::m_bIsScoped);
        if (isScoped) return;   // already scoped

        // Cooldown: don't right-click more than once per 200ms
        static DWORD s_lastScope = 0;
        DWORD now = GetTickCount();
        if (now - s_lastScope < 200u) return;
        s_lastScope = now;

        INPUT inp[2] = {};
        inp[0].type = INPUT_MOUSE; inp[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        inp[1].type = INPUT_MOUSE; inp[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        SendInput(2, inp, sizeof(INPUT));
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void Movement::RunInfiniteDuck()
{
    if (!Globals::misc_infinite_duck) {
        if (s_duck_forced) {
            uintptr_t client = Memory::GetModuleBase("client.dll");
            if (client) __try { *(uint32_t*)(client + BTN_DUCK) = BTN_RELEASED; }
                        __except (EXCEPTION_EXECUTE_HANDLER) {}
            s_duck_forced = false;
        }
        return;
    }

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;

    uintptr_t pawn = 0;
    __try {
        uintptr_t addr = *reinterpret_cast<uintptr_t*>(client + Offsets::dwLocalPlayerPawn);
        if (addr > 0x10000 && addr < 0x7FFFFFFFFFFFull) {
            int32_t hp = *reinterpret_cast<int32_t*>(addr + Offsets::m_iHealth);
            if (hp > 0) pawn = addr;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    if (!pawn) return;

    __try {
        bool duckHeld = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;
        if (duckHeld) {
            *(uint32_t*)(client + BTN_DUCK) = BTN_PRESSED;
            s_duck_forced = true;
        } else if (s_duck_forced) {
            *(uint32_t*)(client + BTN_DUCK) = BTN_RELEASED;
            s_duck_forced = false;
        }
        *(float*)(pawn + Offsets::m_flDuckSpeed) = 8.0f;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
