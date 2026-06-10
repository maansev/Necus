#pragma once
#include <cstdint>

namespace Offsets
{
    // ==================== БАЗОВЫЕ ОФФСЕТЫ (cs2-dumper 2026-06-10) ====================
    constexpr uintptr_t dwEntityList = 0x24E76A0;
    constexpr uintptr_t dwLocalPlayerController = 0x2320720;
    constexpr uintptr_t dwLocalPlayerPawn = 0x2341698;
    constexpr uintptr_t dwViewMatrix = 0x2346B30;
    constexpr uintptr_t dwViewAngles = 0x23568C8;
    constexpr uintptr_t dwInputSystem = 0x42B50;

    // ==================== C_BaseEntity ====================
    constexpr uintptr_t m_pGameSceneNode = 0x330;
    constexpr uintptr_t m_iTeamNum = 0x3EB;
    constexpr uintptr_t m_iHealth = 0x34C;
    constexpr uintptr_t m_fFlags = 0x3F8;
    constexpr uintptr_t m_hOwnerEntity = 0x520;   // обновлён 2026-06-10
    constexpr uintptr_t m_MoveType = 0x525;

    // ==================== C_BaseModelEntity ====================
    constexpr uintptr_t m_modelState = 0x150;
    constexpr uintptr_t m_clrRender = 0xC98;
    constexpr uintptr_t m_Glow = 0xDD8;
    constexpr uintptr_t m_vecViewOffset = 0xE70;

    // ==================== CGlowProperty (relative to m_Glow) ====================
    constexpr uintptr_t m_iGlowType = 0x30;
    constexpr uintptr_t m_iGlowTeam = 0x34;
    constexpr uintptr_t m_nGlowRange = 0x38;
    constexpr uintptr_t m_glowColorOverride = 0x40;
    constexpr uintptr_t m_bEligibleForScreenHighlight = 0x50;
    constexpr uintptr_t m_bGlowing = 0x51;

    // ==================== C_CSPlayerPawn ====================
    constexpr uintptr_t m_vOldOrigin = 0x1390;
    constexpr uintptr_t m_iShotsFired = 0x1C64;
    constexpr uintptr_t m_aimPunchAngle = 0x16E4;
    constexpr uintptr_t m_pWeaponServices = 0x11E0;
    constexpr uintptr_t m_pObserverServices = 0x11F8;
    constexpr uintptr_t m_angEyeAngles = 0x3320;
    constexpr uintptr_t m_flViewmodelOffsetX = 0x1B70;
    constexpr uintptr_t m_flViewmodelOffsetY = 0x1B74;
    constexpr uintptr_t m_flViewmodelOffsetZ = 0x1B78;
    constexpr uintptr_t m_flViewmodelFOV = 0x1B7C;

    // No Flash (C_CSPlayerPawnBase)
    constexpr uintptr_t m_flFlashMaxAlpha = 0x13FC;
    constexpr uintptr_t m_flFlashDuration = 0x1400;

    // Duck / movement
    constexpr uintptr_t m_bDucked = 0x408;
    constexpr uintptr_t m_flDuckAmount = 0x40C;
    constexpr uintptr_t m_vecVelocity = 0x430;
    constexpr uintptr_t m_duckUntilOnGround = 0x438;

    // ==================== C_CSPlayerController ====================
    constexpr uintptr_t m_iszPlayerName = 0x6F4;
    constexpr uintptr_t m_hPlayerPawn = 0x90C;
    constexpr uintptr_t m_bPawnIsAlive = 0x914;

    // CObserverServices (relative to m_pObserverServices ptr)
    constexpr uintptr_t m_iObserverMode = 0x48;
    constexpr uintptr_t m_hObserverTarget = 0x4C;

    // ==================== Оружие / экономика ====================
    constexpr uintptr_t m_hActiveWeapon = 0x60;
    constexpr uintptr_t m_AttributeManager = 0x1180;   // C_AttributeContainer в weapon entity
    constexpr uintptr_t m_iItemDefinitionIndex = 0x1BA;
    constexpr uintptr_t m_nFallbackPaintKit = 0x1658;
    constexpr uintptr_t m_nFallbackSeed = 0x165C;
    constexpr uintptr_t m_flFallbackWear = 0x1660;
    constexpr uintptr_t m_nFallbackStatTrak = 0x1664;
    constexpr uintptr_t m_szCustomName = 0x2F8;
    constexpr uintptr_t m_AttributeList = 0x208;

    // ==================== No Smoke (C_SmokeGrenadeProjectile) ====================
    constexpr uintptr_t m_nSmokeEffectTickBegin = 0x1250;
    constexpr uintptr_t m_bDidSmokeEffect = 0x1254;
    constexpr uintptr_t m_vSmokeColor = 0x125C;
    constexpr uintptr_t m_bSmokeVolumeDataReceived = 0x1298;

    // ==================== C4 (C_PlantedC4, cs2-dumper 2026-06-10) ====================
    constexpr uintptr_t m_bBombTicking = 0x1160;   // обновлён
    constexpr uintptr_t m_flC4Blow = 0x1190;   // обновлён
    constexpr uintptr_t m_flTimerLength = 0x1198;   // обновлён
}
