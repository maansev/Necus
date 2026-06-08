#include "glove_changer.h"
#include "item_schema.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/entity/Classes.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/Offsets.h"

void c_glove_changer::run(int stage) {
    if (stage != 7) return;
    if (!Globals::glove_changer_enabled) return;

    auto* local_pawn = EntityManager::Get().GetLocalPawn();
    if (!local_pawn || !local_pawn->IsAlive()) return;

    // m_EconGloves (C_EconItemView) лежит по смещению 0x1658
    auto* glovesItem = reinterpret_cast<C_EconItemView*>((uintptr_t)local_pawn + 0x1658);
    if (!glovesItem) return;

    const auto& glove = glove_items[Globals::glove_changer_model];
    glovesItem->m_iItemDefinitionIndex() = glove.definition_index;
}