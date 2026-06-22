#include "RageInternal.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/memory/Offsets.h"
#include <Windows.h>

// ─── Weapon resolution + per-weapon profile mapping ──────────────────────────
// Self-contained copies so the rage module never depends on Aimbot internals.
namespace Rage {

// Reads the active weapon's def_index via WeaponServices. Returns 0 on failure.
uint16_t ActiveWeaponDef(C_CSPlayerPawn* pawn)
{
    uint16_t def = 0;
    __try {
        uintptr_t ws = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(pawn) + Offsets::m_pWeaponServices);
        if (!ws) return 0;
        uint32_t handle = *reinterpret_cast<uint32_t*>(ws + Offsets::m_hActiveWeapon);
        if (!handle || handle == 0xFFFFFFFFu) return 0;
        uintptr_t weap = EntityManager::Get().GetEntityFromHandle(handle);
        if (!weap) return 0;
        def = *reinterpret_cast<uint16_t*>(
            weap + Offsets::m_AttributeManager + 0x50 + Offsets::m_iItemDefinitionIndex);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return def;
}

// Current magazine count of the active weapon. Returns -1 when it can't be read
// confidently. The value is range-checked (0..250) so a stale m_iClip1 offset
// reading garbage is reported as "unknown" rather than a false 0 — callers then
// fall back to firing instead of locking up.
int ActiveWeaponClip(C_CSPlayerPawn* pawn)
{
    int clip = -1;
    __try {
        uintptr_t ws = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(pawn) + Offsets::m_pWeaponServices);
        if (!ws) return -1;
        uint32_t handle = *reinterpret_cast<uint32_t*>(ws + Offsets::m_hActiveWeapon);
        if (!handle || handle == 0xFFFFFFFFu) return -1;
        uintptr_t weap = EntityManager::Get().GetEntityFromHandle(handle);
        if (!weap) return -1;
        int raw = *reinterpret_cast<int*>(weap + Offsets::m_iClip1);
        if (raw >= 0 && raw <= 250) clip = raw;   // plausible → trust it
    } __except (EXCEPTION_EXECUTE_HANDLER) { clip = -1; }
    return clip;
}

// Weapons rage must never fire on: Zeus, knives, grenades, C4, breach/bump.
bool IsExcludedWeapon(uint16_t def)
{
    if (def == 31) return true;                  // Zeus
    if (def == 42 || def == 59) return true;     // default knives
    if (def >= 43 && def <= 49) return true;     // grenades + C4
    if (def == 68 || def == 69) return true;     // breach / bump
    if (def >= 500 && def <= 530) return true;   // all CS2 named knives
    return false;
}

// def_index → slot in Globals::g_rageProfiles[] (slot 0 = Global fallback).
// Indices 1-34 match NecusState::kWeapons in Menu.cpp.
int ProfileSlot(uint16_t def)
{
    static const struct { uint16_t d; int s; } kMap[] = {
        { 1,  1}, {64,  2}, { 2,  3}, { 3,  4}, { 4,  5},
        {32,  6}, {61,  7}, {36,  8}, {63,  9}, {30, 10},
        {17, 11}, {34, 12}, {33, 13}, {23, 14}, {24, 15},
        {19, 16}, {26, 17}, {13, 18}, {10, 19}, { 7, 20},
        {16, 21}, {60, 22}, {39, 23}, { 8, 24}, {40, 25},
        { 9, 26}, {11, 27}, {38, 28}, {35, 29}, {25, 30},
        {27, 31}, {29, 32}, {14, 33}, {28, 34}
    };
    for (auto& m : kMap) if (m.d == def) return m.s;
    return 0; // Global
}

} // namespace Rage
