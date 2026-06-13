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
    // Подтверждены 2026-06-12 из дампа.
    constexpr uintptr_t dwGameRules = 0x2340158;  // client.dll → CCSGameRules*
    constexpr uintptr_t m_bTeamIntroPeriod = 0xF04;      // bool — заставка "Первая половина"
    constexpr uintptr_t dwCSGOInput = 0x2355230;  // client.dll → CCSGOInput
    // CCSGOInput::m_bInThirdPerson — НЕ из схемы (класс не дампится). 0x249 —
    // общеизвестное значение для билдов 2025-2026; запись SEH-защищена.
    constexpr uintptr_t m_bInThirdPerson = 0x249;

    // ==================== C_BaseEntity ====================
    constexpr uintptr_t m_pGameSceneNode = 0x330;
    // CGameSceneNode::m_flScale — подтверждён 2026-06-12.
    // Remove Legs: scale≈0 у локальной модели скрывает ноги+тень.
    constexpr uintptr_t m_flScale = 0x40;
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
    constexpr uintptr_t m_flDuckAmount = 0x40C;   // подтверждён 2026-06-12
    constexpr uintptr_t m_flDuckSpeed = 0x410;   // подтверждён 2026-06-12
    constexpr uintptr_t m_vecVelocity = 0x430;
    constexpr uintptr_t m_duckUntilOnGround = 0x438;

    // ==================== C_CSPlayerPawn (scope state, подтверждено из дампа) ====================
    constexpr uintptr_t m_bIsScoped = 0x1C50;   // bool
    // m_flShadowStrength (C_BaseModelEntity) — подтверждён из дампа
    constexpr uintptr_t m_flShadowStrength = 0xE40;
    // CBasePlayerPawn::m_iHideHUD — битмаска HUD; бит 256 = HIDEHUD_CROSSHAIR. Подтверждён 2026-06-12.
    constexpr uintptr_t m_iHideHUD = 0x12B0;

    // ==================== C_CSPlayerController ====================
    constexpr uintptr_t m_iszPlayerName = 0x6F4;
    constexpr uintptr_t m_hPlayerPawn = 0x90C;
    constexpr uintptr_t m_bPawnIsAlive = 0x914;
    // CCSPlayerController::m_pInGameMoneyServices — ПОДТВЕРДИТЬ из дампа
    constexpr uintptr_t m_pInGameMoneyServices = 0x8D8;
    // CCSPlayerController_InGameMoneyServices::m_iAccount
    constexpr uintptr_t m_iAccount = 0x40;

    // ==================== Player info (Show Money) ====================
    // CBasePlayerPawn::m_pItemServices (между WeaponServices 0x11E0 и ObserverServices 0x11F8)
    constexpr uintptr_t m_pItemServices = 0x11E8;
    // CCSPlayer_ItemServices
    constexpr uintptr_t m_bHasDefuser = 0x48;
    constexpr uintptr_t m_bHasHelmet = 0x49;
    // C_CSPlayerPawn::m_ArmorValue — 0 = отключено, нужна строка из дампа
    constexpr uintptr_t m_ArmorValue = 0x0;
    // CPlayer_WeaponServices::m_hMyWeapons (CNetworkUtlVectorBase: count +0x0, data ptr +0x8)
    constexpr uintptr_t m_hMyWeapons = 0x8;

    // CObserverServices (relative to m_pObserverServices ptr)
    constexpr uintptr_t m_iObserverMode = 0x48;
    constexpr uintptr_t m_hObserverTarget = 0x4C;

    // ==================== Camera services (Thirdperson) ====================
    // Подтверждён 2026-06-12 из дампа.
    constexpr uintptr_t m_pCameraServices = 0x1218;   // C_BasePlayerPawn → CPlayer_CameraServices*
    // Относительно m_pCameraServices. 0x0 = не известен — нужна строка из дампа:
    //   CPlayer_CameraServices / CCSPlayerBase_CameraServices camera-origin field
    constexpr uintptr_t m_vecCameraOffset = 0x0;

    // ==================== Movement services (Infinite Duck) ====================
    // m_flDuckAmount и m_flDuckSpeed — ПРЯМЫЕ оффсеты на pawn (не через сервисный указатель).
    // Подтверждено 2026-06-12: m_flDuckAmount=0x40C уже был; m_flDuckSpeed=0x410 обновлён.
    constexpr uintptr_t m_pMovementServices = 0x0;   // не нужен — поля прямые на pawn
    constexpr uintptr_t m_flDuckSpeed_Svc = 0x0;   // не используется

    // ==================== Steam / econ identity ====================
    // CBasePlayerController::m_steamID (uint64) — подтверждён 2026-06-12.
    constexpr uintptr_t m_steamID = 0x780;
    // C_EconItemView::m_iAccountID (confirmed cs2-dumper 2026-06-03)
    constexpr uintptr_t m_iAccountID = 0x1D8;

    // ==================== Оружие / экономика ====================
    // C_WeaponCSBaseStatTrak VData — CFiringModeFloat (struct{float[2]}): [0]=normal, [1]=silenced/scoped.
    // Подтверждены 2026-06-12 из дампа.
    constexpr uintptr_t m_flSpread = 0x758;  // CFiringModeFloat
    constexpr uintptr_t m_flInaccuracyCrouch = 0x760;  // CFiringModeFloat
    constexpr uintptr_t m_flInaccuracyStand = 0x768;  // CFiringModeFloat
    constexpr uintptr_t m_flInaccuracyJump = 0x770;  // CFiringModeFloat
    constexpr uintptr_t m_flInaccuracyLand = 0x778;  // CFiringModeFloat
    constexpr uintptr_t m_flInaccuracyFire = 0x788;  // CFiringModeFloat (recoil per shot)
    constexpr uintptr_t m_flInaccuracyMove = 0x790;  // CFiringModeFloat (movement penalty)
    constexpr uintptr_t m_flInaccuracy = 0x0;    // вычисляемое — не хранится напрямую
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
