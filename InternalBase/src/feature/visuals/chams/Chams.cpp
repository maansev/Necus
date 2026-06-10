#include "Chams.h"
#include "../../../sdk/utils/Globals.h"
#include "../../../sdk/entity/EntityManager.h"
#include "../../../sdk/entity/Classes.h"
#include <windows.h>

// C_BaseModelEntity::m_clrRender (RGBA uint8 x4, offset stable since CS2 launch)
static constexpr uintptr_t kClrRenderOff = 0x70;

static inline void WriteColor(uintptr_t pawn, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    uintptr_t addr = pawn + kClrRenderOff;
    __try {
        uint8_t* p = reinterpret_cast<uint8_t*>(addr);
        p[0] = r; p[1] = g; p[2] = b; p[3] = a;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void chams::Update()
{
    auto& em = EntityManager::Get();

    const bool anyEnabled = Globals::enemy_chams_enabled || Globals::team_chams_enabled;

    // Iterate shared entities list (same lock-then-return pattern as ESP)
    const auto& entities = em.GetEntities();

    for (const auto& ent : entities)
    {
        if (!ent.pawn) continue;

        bool isEnemy = ent.isEnemy;
        bool enabled = isEnemy ? Globals::enemy_chams_enabled : Globals::team_chams_enabled;

        if (enabled)
        {
            const float* c = isEnemy
                ? Globals::enemy_chams_visible_color
                : Globals::team_chams_visible_color;

            WriteColor(
                reinterpret_cast<uintptr_t>(ent.pawn),
                static_cast<uint8_t>(c[0] * 255.f),
                static_cast<uint8_t>(c[1] * 255.f),
                static_cast<uint8_t>(c[2] * 255.f),
                255u
            );
        }
        else
        {
            // Restore to game default (white, full alpha)
            WriteColor(reinterpret_cast<uintptr_t>(ent.pawn), 255, 255, 255, 255);
        }
    }
}
