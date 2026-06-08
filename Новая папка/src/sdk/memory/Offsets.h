#pragma once
#include <cstdint>

namespace Offsets
{
    // ==================== ¡¿«Œ¬€≈ Œ‘‘—≈“€ (ËÁ cs2-dumper) ====================
    constexpr uintptr_t dwEntityList = 0x24E6590;
    constexpr uintptr_t dwLocalPlayerPawn = 0x2340698;
    constexpr uintptr_t dwViewMatrix = 0x2345B30;

    // ==================== ƒÀﬂ CHAMS ====================
    constexpr uintptr_t m_pGameSceneNode = 0x330;   // C_BaseEntity -> CGameSceneNode*
    constexpr uintptr_t m_modelState = 0x150;   // CBodyComponentSkeletonInstance -> modelState
    constexpr uintptr_t m_iTeamNum = 0x3EB;   // C_BaseEntity
    constexpr uintptr_t m_iHealth = 0x34C;   // C_BaseEntity
    constexpr uintptr_t m_hOwnerEntity = 0x4C0;   // C_BaseEntity
    constexpr uintptr_t m_pWeaponServices = 0x11E0;  // C_CSPlayerPawn

    // ==================== “¬Œ» —”Ÿ≈—“¬”ﬁŸ»≈ Œ‘‘—≈“€ ====================
    constexpr uintptr_t m_vOldOrigin = 0x1390;
    constexpr uintptr_t m_vecViewOffset = 0xE70;
    constexpr uintptr_t m_iShotsFired = 0x1C64;
    constexpr uintptr_t m_aimPunchAngle = 0x16E4;
    constexpr uintptr_t m_iszPlayerName = 0x6F4;
    constexpr uintptr_t m_hPlayerPawn = 0x90C;
    constexpr uintptr_t m_bPawnIsAlive = 0x914;
    constexpr uintptr_t m_fFlags = 0x3F8;
    constexpr uintptr_t m_iItemDefinitionIndex = 0x1BA;
    constexpr uintptr_t m_hActiveWeapon = 0x60;
    constexpr uintptr_t m_AttributeManager = 0x1180;

    // ==================== GLOW (Û ÚÂ·ˇ ‡·ÓÚ‡ÂÚ) ====================
    constexpr uintptr_t m_Glow = 0xDD8;
    constexpr uintptr_t m_iGlowType = 0x30;
    constexpr uintptr_t m_iGlowTeam = 0x34;
    constexpr uintptr_t m_nGlowRange = 0x38;
    constexpr uintptr_t m_glowColorOverride = 0x40;
    constexpr uintptr_t m_bEligibleForScreenHighlight = 0x50;
    constexpr uintptr_t m_bGlowing = 0x51;

    // ==================== ƒ–”√»≈ ====================
    constexpr uintptr_t dwInputSystem = 0x42B50;

    // CObserverServices
    constexpr std::ptrdiff_t m_pObserverServices = 0x11F8;
    constexpr std::ptrdiff_t m_hObserverTarget = 0x4C;
    constexpr std::ptrdiff_t m_iObserverMode = 0x48;

    constexpr uintptr_t m_MoveType = 0x525; // C_BaseEntity
}