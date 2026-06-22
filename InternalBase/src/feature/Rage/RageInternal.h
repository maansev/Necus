#pragma once
#include "../../sdk/entity/Classes.h"
#include "../../sdk/utils/Vector.h"
#include <cstdint>

// Cross-file declarations shared between the rage/ translation units. Not part of
// the public surface — only the rage submodules include this.
namespace Rage {

    // Per-frame context resolved once by Rage::Run and passed to every submodule
    // so none of them re-read the active weapon or re-resolve the profile slot.
    struct Context {
        C_CSPlayerPawn* local      = nullptr;
        Vector          eye        = {};      // local eye position (origin + view offset)
        Vector          view       = {};      // current view angles
        uint16_t        weaponDef  = 0;       // active weapon def_index
        int             slot       = 0;       // index into Globals::g_rageProfiles[]
    };

    // ── RageProfile.cpp ──────────────────────────────────────────────────────
    uint16_t ActiveWeaponDef(C_CSPlayerPawn* pawn);
    bool     IsExcludedWeapon(uint16_t def);   // knives / grenades / Zeus
    int      ProfileSlot(uint16_t def);        // def_index → g_rageProfiles slot

    // Current rounds in the active weapon's magazine. Returns -1 when the clip
    // count can't be read reliably (so callers treat "unknown" as "may fire").
    int      ActiveWeaponClip(C_CSPlayerPawn* pawn);

    // ── RageTarget.cpp ───────────────────────────────────────────────────────
    // Picks the best enemy and the best aim point on it (honouring fov, hitboxes
    // and multipoint from the active rage profile). Returns nullptr if none pass.
    C_CSPlayerPawn* SelectTarget(const Context& ctx, Vector& outPoint);

    // ── RageAutoWall.cpp ─────────────────────────────────────────────────────
    // Estimated damage the active weapon lands on `point` of `target` from the
    // local eye — distance falloff + armor. Returns 0 when the shot is pointless.
    // (A true penetration trace needs an engine TraceRay export the SDK lacks; the
    // hook here is structured so that can be slotted in without touching callers.)
    float EstimateDamage(const Context& ctx, C_CSPlayerPawn* target, const Vector& point);

    // Base damage of the active weapon (used by EstimateDamage and the gate).
    float WeaponBaseDamage(uint16_t def);

    // ── RageAutoFire.cpp ─────────────────────────────────────────────────────
    void AutoFire();   // rate-limited left click when a shot is justified

} // namespace Rage
