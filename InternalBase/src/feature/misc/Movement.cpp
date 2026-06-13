#include "Movement.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/memory/Offsets.h"
#include <Windows.h>

static float s_prevYaw = 0.f;
static bool  s_yawInit = false;
static bool  s_duck_forced = false;
static float s_lastMouseDelta = 0.f;

void Movement::RunAutostrafer()
{
    if (!Globals::misc_autostrafer) return;

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;

    auto* pawn = EntityManager::Get().GetLocalPawn();
    if (!pawn || !pawn->IsAlive()) return;

    uint8_t moveType = *(uint8_t*)((uintptr_t)pawn + Offsets::m_MoveType);
    if (moveType == 9 || moveType == 8) return; // ladder / noclip

    int flags = *(int*)((uintptr_t)pawn + Offsets::m_fFlags);
    bool onGround = (flags & 1) != 0;
    if (onGround) {
        s_yawInit = false;
        return;
    }

    uintptr_t L = client + 0x2065DF0;
    uintptr_t R = client + 0x2065E80;

    float* va = reinterpret_cast<float*>(client + 0x23568C8);
    float curYaw = va[1];

    if (!s_yawInit) {
        s_prevYaw = curYaw;
        s_yawInit = true;
        return;
    }

    float mouseDelta = curYaw - s_prevYaw;
    while (mouseDelta > 180.f) mouseDelta -= 360.f;
    while (mouseDelta < -180.f) mouseDelta += 360.f;

    s_prevYaw = curYaw;

    bool holdingA = (GetAsyncKeyState('A') & 0x8000) != 0;
    bool holdingD = (GetAsyncKeyState('D') & 0x8000) != 0;
    bool holdingSpace = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

    float smooth = max(0.08f, Globals::misc_autostrafer_smooth);
    float threshold = 0.04f / smooth;

    // === Основная логика (как ты просил) ===
    if (holdingD && !holdingA) {
        *(uint32_t*)R = 0x10001;
        *(uint32_t*)L = 0x100;
    }
    else if (holdingA && !holdingD) {
        *(uint32_t*)L = 0x10001;
        *(uint32_t*)R = 0x100;
    }
    else if (holdingSpace && !holdingA && !holdingD) {
        // Только пробел — толкаем вперёд (авто-стрейф вперёд)
        if (fabsf(mouseDelta) < 0.5f) {
            *(uint32_t*)R = 0x10001;
            *(uint32_t*)L = 0x100;
        }
    }
    else {
        // Реакция на мышку
        if (mouseDelta > threshold) {
            *(uint32_t*)R = 0x10001;
            *(uint32_t*)L = 0x100;
        }
        else if (mouseDelta < -threshold) {
            *(uint32_t*)L = 0x10001;
            *(uint32_t*)R = 0x100;
        }
        else {
            *(uint32_t*)L = 0x100;
            *(uint32_t*)R = 0x100;
        }
    }
}

void Movement::RunInfiniteDuck()
{
    if (!Globals::misc_infinite_duck) {
        if (s_duck_forced) {
            uintptr_t client = Memory::GetModuleBase("client.dll");
            if (client) *(uint32_t*)(client + 0x2066030) = 0x100;
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
        *(uint32_t*)(client + 0x2066030) = 0x10001;
        s_duck_forced = true;
    }
    else if (s_duck_forced) {
        *(uint32_t*)(client + 0x2066030) = 0x100;
        s_duck_forced = false;
    }

    // Постоянно держим максимальную скорость приседа
    *(float*)((uintptr_t)pawn + Offsets::m_flDuckSpeed) = 8.0f;
}