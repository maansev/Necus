#include "Chams.h"
#include "../../../sdk/utils/Globals.h"
#include "../../../sdk/memory/Offsets.h"
#include "../../../sdk/entity/EntityManager.h"
#include "../../../sdk/entity/Classes.h"
#include <algorithm>
#include <windows.h>
#include <cstdint>

static inline void SetGlow(uintptr_t pawn, const float* col, int glowType)
{
    __try {
        uintptr_t glow = pawn + Offsets::m_Glow;
        *reinterpret_cast<bool*>(glow + Offsets::m_bGlowing) = true;
        *reinterpret_cast<int32_t*>(glow + Offsets::m_iGlowType) = glowType;
        *reinterpret_cast<uint8_t*>(glow + Offsets::m_glowColorOverride + 0) = static_cast<uint8_t>(std::clamp(col[0] * 255.f, 0.f, 255.f));
        *reinterpret_cast<uint8_t*>(glow + Offsets::m_glowColorOverride + 1) = static_cast<uint8_t>(std::clamp(col[1] * 255.f, 0.f, 255.f));
        *reinterpret_cast<uint8_t*>(glow + Offsets::m_glowColorOverride + 2) = static_cast<uint8_t>(std::clamp(col[2] * 255.f, 0.f, 255.f));
        *reinterpret_cast<uint8_t*>(glow + Offsets::m_glowColorOverride + 3) = 255u;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static inline void ClearGlow(uintptr_t pawn)
{
    __try {
        uintptr_t glow = pawn + Offsets::m_Glow;
        *reinterpret_cast<bool*>(glow + Offsets::m_bGlowing) = false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void chams::Update()
{
    auto& em = EntityManager::Get();
    const auto& entities = em.GetEntities();

    static bool s_wasActive = false;
    const bool anyEnabled = Globals::enemy_chams_enabled || Globals::team_chams_enabled;

    if (!anyEnabled) {
        if (s_wasActive) {
            for (const auto& ent : entities)
                if (ent.pawn) ClearGlow(reinterpret_cast<uintptr_t>(ent.pawn));
            s_wasActive = false;
        }
        return;
    }
    s_wasActive = true;

    // Glow type 3 = solid full-model fill (chams-like), type 1 = outline
    const int kChamsGlowType = 3;

    for (const auto& ent : entities)
    {
        if (!ent.pawn) continue;
        uintptr_t pawn = reinterpret_cast<uintptr_t>(ent.pawn);

        bool isEnemy = ent.isEnemy;
        bool enabled = isEnemy ? Globals::enemy_chams_enabled : Globals::team_chams_enabled;

        if (enabled) {
            const float* col = isEnemy
                ? Globals::enemy_chams_visible_color
                : Globals::team_chams_visible_color;
            SetGlow(pawn, col, kChamsGlowType);
        }
        else {
            ClearGlow(pawn);
        }
    }
}
