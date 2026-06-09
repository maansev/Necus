#include "glove_changer.h"
#include "item_schema.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/entity/Classes.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/Offsets.h"
#include <Windows.h>

void c_glove_changer::run(int stage)
{
    if (stage != 3) return;
    if (!Globals::glove_changer_enabled) return;

    int model = Globals::glove_changer_model;
    if (model < 0 || model >= (int)glove_items.size()) return;
    const auto& glove = glove_items[model];

    __try
    {
        auto* local = EntityManager::Get().GetLocalPawn();
        if (!local || !local->IsAlive()) return;

        // m_EconGloves (inline C_EconItemView) at C_CSPlayerPawn + 0x1658
        auto* glovesItem = reinterpret_cast<C_EconItemView*>(
            reinterpret_cast<uintptr_t>(local) + 0x1658);
        glovesItem->m_iItemDefinitionIndex() = glove.definition_index;

        auto* gloveEcon = reinterpret_cast<C_EconEntity*>(
            reinterpret_cast<uintptr_t>(local) + 0x1658);
        gloveEcon->m_nFallbackPaintKit() = GetPaintKitId(Globals::glove_changer_paint_kit);
        gloveEcon->m_flFallbackWear() = Globals::glove_changer_wear;
        gloveEcon->m_nFallbackSeed() = Globals::glove_changer_seed;

        // m_bNeedToReApplyGloves (0x1655) Ч движок примен€ет новую модель
        *reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(local) + 0x1655) = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
