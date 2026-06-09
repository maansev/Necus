#pragma once
#include <cstdint>

namespace Offsets
{
    // ─── POINTER OFFSETS (из cs2-dumper, дамп 2026-06-03) ────────────────────
    constexpr uintptr_t dwEntityList = 0x24E6590;
    constexpr uintptr_t dwLocalPlayerPawn = 0x2340698;
    constexpr uintptr_t dwViewMatrix = 0x2345B30;

    // ─── ENTITY LIST — ВАЖНО! ────────────────────────────────────────────────
    // Размер одной записи CEntityIdentity в чанке списка сущностей.
    // В CS2 начиная с середины 2024 это 120 (0x78), НЕ 112 (0x70).
    // Если видишь ноль сущностей при правильных pointer-оффсетах — это первое,
    // что нужно проверить. Исходный код в EntityManager.cpp использовал 112
    // (старое значение) — вот почему ESP/glow отвалились после обновления CS2.
    constexpr uintptr_t kEntityStride = 120;   // 0x78 — подтверждено для CS2 ~2024-2026

    // ─── PLAYER ───────────────────────────────────────────────────────────────
    constexpr uintptr_t m_pGameSceneNode = 0x330;
    constexpr uintptr_t m_iTeamNum = 0x3EB;
    constexpr uintptr_t m_iHealth = 0x34C;
    constexpr uintptr_t m_fFlags = 0x3F8;
    constexpr uintptr_t m_hOwnerEntity = 0x4C0;
    constexpr uintptr_t m_pWeaponServices = 0x11E0;   // C_BasePlayerPawn (подтверждено дампом)
    constexpr uintptr_t m_vOldOrigin = 0x1390;   // C_BasePlayerPawn (подтверждено)
    constexpr uintptr_t m_vecViewOffset = 0xE70;    // (подтверждено)
    constexpr uintptr_t m_iShotsFired = 0x1C64;   // C_CSPlayerPawn  (подтверждено)
    constexpr uintptr_t m_aimPunchAngle = 0x16E4;
    constexpr uintptr_t m_pObserverServices = 0x11F8;
    constexpr uintptr_t m_MoveType = 0x525;

    // ─── CONTROLLER ───────────────────────────────────────────────────────────
    constexpr uintptr_t m_hPlayerPawn = 0x90C;    // C_CSPlayerController (подтверждено)
    constexpr uintptr_t m_bPawnIsAlive = 0x914;    // (подтверждено)
    constexpr uintptr_t m_iszPlayerName = 0x6F4;    // char[128] (подтверждено)

    // ─── SCENE / MODEL ────────────────────────────────────────────────────────
    constexpr uintptr_t m_modelState = 0x150;    // CSkeletonInstance (подтверждено)

    // ─── GLOW ────────────────────────────────────────────────────────────────
    constexpr uintptr_t m_Glow = 0xDD8;    // CGlowProperty (подтверждено дампом)
    constexpr uintptr_t m_iGlowType = 0x30;
    constexpr uintptr_t m_iGlowTeam = 0x34;
    constexpr uintptr_t m_nGlowRange = 0x38;
    constexpr uintptr_t m_glowColorOverride = 0x40;
    constexpr uintptr_t m_bEligibleForScreenHighlight = 0x50;
    constexpr uintptr_t m_bGlowing = 0x51;

    // ─── WEAPON / ECONOMY ────────────────────────────────────────────────────
    constexpr uintptr_t m_hActiveWeapon = 0x60;
    constexpr uintptr_t m_AttributeManager = 0x1180;   // C_EconEntity (подтверждено)
    constexpr uintptr_t m_iItemDefinitionIndex = 0x1BA;
    constexpr uintptr_t m_nFallbackPaintKit = 0x1658;
    constexpr uintptr_t m_nFallbackSeed = 0x165C;
    constexpr uintptr_t m_flFallbackWear = 0x1660;
    constexpr uintptr_t m_nFallbackStatTrak = 0x1664;
    constexpr uintptr_t m_szCustomName = 0x2F8;
    constexpr uintptr_t m_AttributeList = 0x208;

    // ─── OBSERVER ────────────────────────────────────────────────────────────
    constexpr uintptr_t m_hObserverTarget = 0x4C;
    constexpr uintptr_t m_iObserverMode = 0x48;

    // ─── MISC ────────────────────────────────────────────────────────────────
    constexpr uintptr_t dwInputSystem = 0x42B50;
}
