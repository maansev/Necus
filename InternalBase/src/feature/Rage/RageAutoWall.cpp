#include "RageInternal.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/Offsets.h"
#include <Windows.h>
#include <cmath>

// ─── Auto-wall / damage estimate ─────────────────────────────────────────────
// CS2 has no exposed bullet trace, so a true penetration calculation (wall
// thickness, surface penetration modifier, bullets retained) isn't possible from
// the SDK as-is. What we CAN model accurately is the part that decides whether a
// shot is worth taking: weapon base damage, distance falloff and armor. When the
// path is blocked we apply a flat penetration penalty so "Aim Through Walls"
// shots still respect the Min Damage gate instead of firing blindly.
//
// EstimateDamage is the single seam a real TraceRay would plug into later:
// replace `blocked`/`penFactor` with trace-derived values and every caller keeps
// working unchanged.
namespace Rage {

// Base damage per weapon def_index. Values are the CS2 published per-bullet
// damage; anything not listed falls back to a sane rifle-ish default.
float WeaponBaseDamage(uint16_t def)
{
    switch (def) {
        case 7:  return 36.f;  // AK-47
        case 8:  return 30.f;  // AUG
        case 9:  return 115.f; // AWP
        case 10: return 38.f;  // FAMAS
        case 11: return 30.f;  // G3SG1
        case 13: return 30.f;  // Galil AR
        case 14: return 35.f;  // M249
        case 16: return 33.f;  // M4A4
        case 60: return 33.f;  // M4A1-S
        case 17: return 26.f;  // MAC-10
        case 19: return 26.f;  // P90
        case 23: return 26.f;  // MP5-SD
        case 24: return 23.f;  // UMP-45
        case 25: return 32.f;  // XM1014
        case 26: return 27.f;  // Bizon
        case 27: return 26.f;  // MAG-7
        case 28: return 30.f;  // Negev
        case 29: return 26.f;  // Sawed-Off
        case 33: return 25.f;  // MP7
        case 34: return 22.f;  // MP9
        case 35: return 80.f;  // SSG 08 (Scout)
        case 38: return 88.f;  // SCAR-20
        case 39: return 35.f;  // SG 553
        case 40: return 26.f;  // SSG / scoped variants
        // Pistols
        case 1:  return 63.f;  // Deagle
        case 2:  return 38.f;  // Dual Berettas
        case 3:  return 32.f;  // Five-SeveN
        case 4:  return 30.f;  // Glock-18
        case 30: return 33.f;  // Tec-9
        case 32: return 35.f;  // P2000
        case 36: return 38.f;  // P250
        case 61: return 35.f;  // USP-S
        case 63: return 31.f;  // CZ75-Auto
        case 64: return 86.f;  // R8 Revolver
        default: return 30.f;
    }
}

static int SafeArmor(C_CSPlayerPawn* pawn) {
    __try {
        return *reinterpret_cast<int*>(
            reinterpret_cast<uintptr_t>(pawn) + Offsets::m_ArmorValue);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// True when nobody has the target spotted — our only "is the path blocked" proxy
// without a real trace. Treated as blocked → penetration penalty applies.
static bool IsBlocked(C_CSPlayerPawn* pawn) {
    __try {
        bool spotted = *reinterpret_cast<bool*>(
            reinterpret_cast<uintptr_t>(pawn) + Offsets::m_entitySpottedState + 0x8);
        return !spotted;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

float EstimateDamage(const Context& ctx, C_CSPlayerPawn* target, const Vector& point)
{
    float dmg = WeaponBaseDamage(ctx.weaponDef);

    // Distance falloff — CS2 reduces damage with range. ~0.85 per 500u is a close
    // approximation of the average rifle falloff curve.
    float dx = point.x - ctx.eye.x;
    float dy = point.y - ctx.eye.y;
    float dz = point.z - ctx.eye.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    dmg *= std::pow(0.85f, dist / 500.f);

    // Armor — armored bodies eat roughly half of what gets past, capped by the
    // armor remaining. (Head armor isn't separated here; a coarse but useful gate.)
    if (SafeArmor(target) > 0)
        dmg *= 0.78f;

    // Blocked path (no line of sight) → flat penetration penalty so Min Damage
    // still governs wall shots. Replace with a real trace result when available.
    if (IsBlocked(target))
        dmg *= 0.45f;

    return dmg < 0.f ? 0.f : dmg;
}

} // namespace Rage
