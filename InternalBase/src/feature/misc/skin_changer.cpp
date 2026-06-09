#include "skin_changer.h"
#include "item_schema.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/Offsets.h"
#include <cstring>
#include <Windows.h>

// =============================================================================
//  skin_changer.cpp  — пуленепробиваемая версия
//
//  Главные исправления vs оригинал:
//   1) stage == 3 (в CS2 нет stage 7 — оригинал никогда не запускался)
//   2) Верхняя проверка enabled ДО любого обращения к памяти
//   3) Ограничение m_size <= 64 (защита от мусорного значения)
//   4) SEH (__try/__except) — любое нарушение доступа поглощается
// =============================================================================

void c_skin_changer::apply_weapon_skin(C_BasePlayerWeapon* weapon, int paint_kit,
    float wear, int seed, const char* custom_name)
{
    if (!weapon) return;
    weapon->m_nFallbackPaintKit() = paint_kit;
    weapon->m_nFallbackSeed() = seed;
    weapon->m_flFallbackWear() = wear;
    weapon->m_nFallbackStatTrak() = 0;

    if (custom_name && custom_name[0] != '\0') {
        auto* attr = weapon->GetAttrContainer();
        if (attr) {
            auto* item = attr->GetItemPtr();
            if (item) strncpy_s(item->m_szCustomName(), 161, custom_name, 160);
        }
    }
}

void c_skin_changer::apply_knife_skin(C_BasePlayerWeapon* weapon, int knife_index,
    int paint_kit, float wear, int seed)
{
    if (!weapon || knife_index < 0 || knife_index >= (int)knife_items.size()) return;
    auto* attr = weapon->GetAttrContainer();
    if (attr) {
        auto* item = attr->GetItemPtr();
        if (item) item->m_iItemDefinitionIndex() = knife_items[knife_index].definition_index;
    }
    apply_weapon_skin(weapon, paint_kit, wear, seed, nullptr);
}

void c_skin_changer::run(int stage)
{
    // CS2: FRAME_NET_UPDATE_POSTDATAUPDATE_END = 3  (stage 7 не существует!)
    if (stage != 3) return;

    // ── ВЕРХНЯЯ ЗАЩИТА: не трогаем память пока всё выключено ──────────────────
    if (!Globals::skin_changer_enabled && !Globals::knife_changer_enabled) return;

    __try
    {
        auto* local_pawn = EntityManager::Get().GetLocalPawn();
        if (!local_pawn || !local_pawn->IsAlive()) return;

        uintptr_t weapSvc = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(local_pawn) + Offsets::m_pWeaponServices);
        if (!weapSvc) return;

        auto* myWeapons = reinterpret_cast<CUtlVectorSimple*>(weapSvc + 0x48);
        int count = myWeapons->m_size;
        if (count <= 0 || count > 64 || !myWeapons->m_elements) return;

        auto* handles = reinterpret_cast<CHandleSimple*>(myWeapons->m_elements);

        for (int i = 0; i < count; ++i)
        {
            uint32_t handle = handles[i].m_handle;
            if (!handle || handle == 0xFFFFFFFF) continue;

            uintptr_t weapPtr = EntityManager::Get().GetEntityFromHandle(handle);
            if (!weapPtr) continue;

            auto* weapon = reinterpret_cast<C_BasePlayerWeapon*>(weapPtr);
            auto* attr = weapon->GetAttrContainer();
            if (!attr) continue;
            auto* item = attr->GetItemPtr();
            if (!item) continue;

            uint16_t defIdx = item->m_iItemDefinitionIndexC();
            bool isKnife = (defIdx >= 500 && defIdx <= 525) || defIdx == 59;

            if (isKnife && Globals::knife_changer_enabled)
            {
                apply_knife_skin(weapon,
                    Globals::knife_changer_model,
                    GetPaintKitId(Globals::knife_changer_paint_kit),
                    Globals::knife_changer_wear,
                    Globals::knife_changer_seed);
            }
            else if (!isKnife && Globals::skin_changer_enabled)
            {
                apply_weapon_skin(weapon,
                    GetPaintKitId(Globals::skin_changer_paint_kit),
                    Globals::skin_changer_wear,
                    Globals::skin_changer_seed,
                    Globals::skin_changer_custom_name);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { /* нарушение доступа поглощено */ }
}
