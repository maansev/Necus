#include "NoFlash.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/Offsets.h"
#include "../../sdk/entity/EntityManager.h"
#include <cstdint>
#include <Windows.h>

void NoFlash::Init() {}

void NoFlash::Update()
{
    if (!Globals::misc_noflash) return;

    auto* pawn = EntityManager::Get().GetLocalPawn();
    if (!pawn || !pawn->IsAlive()) return;

    uintptr_t base = reinterpret_cast<uintptr_t>(pawn);

    __try {
        *reinterpret_cast<float*>(base + Offsets::m_flFlashDuration) = 0.f;
        *reinterpret_cast<float*>(base + Offsets::m_flFlashMaxAlpha) = 0.f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
