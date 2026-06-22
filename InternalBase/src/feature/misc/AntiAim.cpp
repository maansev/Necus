#include "AntiAim.h"
#include "../../sdk/utils/Globals.h"
#include <Windows.h>
#include <cmath>

static float NormYaw(float a) {
    while (a >  180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}

bool AntiAim::Compute(float realPitch, float realYaw, float& out_pitch, float& out_yaw)
{
    if (!Globals::aa_enabled)
        return false;

    // Key gate: 0 = always on, otherwise require the bound key held
    if (Globals::aa_key != 0 && !(GetAsyncKeyState(Globals::aa_key) & 0x8000))
        return false;

    // Per-tick counters for spin / jitter. CreateMove is the call site (~64Hz).
    static int   s_tick       = 0;
    static bool  s_jitterSide = false;
    static float s_spin       = 0.f;
    s_tick++;
    if ((s_tick & 1) == 0) s_jitterSide = !s_jitterSide;

    // ── Pitch ─────────────────────────────────────────────────────────────────
    float pitch = realPitch;
    switch (Globals::aa_pitch) {
        case 1: pitch =  89.f; break;   // Down  (fake looking at floor)
        case 2: pitch = -89.f; break;   // Up    (fake looking at ceiling)
        case 3: pitch =   0.f; break;   // Zero  (straight ahead)
        case 4: pitch =  84.f; break;   // Minimal (subtle down — less obvious)
        case 0: default: break;         // Off — keep real pitch
    }

    // ── Yaw ───────────────────────────────────────────────────────────────────
    float yaw = realYaw;
    switch (Globals::aa_yaw) {
        case 1: // Backward — face 180° away from real aim
            yaw = realYaw + 180.f;
            break;
        case 2: // Spin — continuously rotate
            s_spin += Globals::aa_spin_speed;
            if (s_spin >  360.f) s_spin -= 360.f;
            yaw = realYaw + s_spin;
            break;
        case 3: // Static — fixed offset from real aim
            yaw = realYaw + Globals::aa_yaw_base;
            break;
        case 4: // Jitter — alternate left/right around the back angle each tick
            yaw = realYaw + 180.f + (s_jitterSide ? Globals::aa_jitter : -Globals::aa_jitter);
            break;
        case 0: default: // Off — keep real yaw
            yaw = realYaw;
            break;
    }

    // If both modes are Off there's nothing to fake.
    if (Globals::aa_pitch == 0 && Globals::aa_yaw == 0)
        return false;

    out_pitch = pitch;
    out_yaw   = NormYaw(yaw);
    return true;
}
