#include "skin_changer.h"
#include "item_schema.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/Offsets.h"
#include <cstring>

void c_skin_changer::apply_weapon_skin(C_BasePlayerWeapon* weapon, int paint_kit, float wear, int seed, const char* custom_name) {
    if (!weapon) return;

    weapon->m_nFallbackPaintKit() = paint_kit;
    weapon->m_nFallbackSeed() = seed;
    weapon->m_flFallbackWear() = wear;
    weapon->m_nFallbackStatTrak() = 0;

    if (custom_name && custom_name[0] != '\0') {
        auto* attr = weapon->GetAttrContainer();
        if (attr) {
            auto* item = attr->GetItemPtr();
            if (item) {
                strncpy_s(item->m_szCustomName(), 161, custom_name, 160);
            }
        }
    }
}

void c_skin_changer::apply_knife_skin(C_BasePlayerWeapon* weapon, int knife_index, int paint_kit, float wear, int seed) {
    if (!weapon || knife_index < 0 || knife_index >= (int)knife_items.size()) return;
    const auto& knife = knife_items[knife_index];

    auto* attr = weapon->GetAttrContainer();
    if (attr) {
        auto* item = attr->GetItemPtr();
        if (item) {
            item->m_iItemDefinitionIndex() = knife.definition_index;
        }
    }

    apply_weapon_skin(weapon, paint_kit, wear, seed, nullptr);
}

void c_skin_changer::run(int stage) {
    if (stage != 7) return;

    auto* local_pawn = EntityManager::Get().GetLocalPawn();
    if (!local_pawn || !local_pawn->IsAlive()) return;

    uintptr_t weaponServices = *(uintptr_t*)((uintptr_t)local_pawn + Offsets::m_pWeaponServices);
    if (!weaponServices) return;

    auto* myWeapons = (CUtlVectorSimple*)(weaponServices + 0x48);
    if (!myWeapons || myWeapons->m_size <= 0) return;

    for (int i = 0; i < myWeapons->m_size; ++i) {
        auto* handles = (CHandleSimple*)myWeapons->m_elements;
        uint32_t handle = handles[i].m_handle;
        if (!handle || handle == 0xFFFFFFFF) continue;

        uintptr_t weaponPtr = EntityManager::Get().GetEntityFromHandle(handle);
        if (!weaponPtr) continue;

        auto* weapon = reinterpret_cast<C_BasePlayerWeapon*>(weaponPtr);
        auto* attr = weapon->GetAttrContainer();
        if (!attr) continue;
        auto* item = attr->GetItemPtr();
        if (!item) continue;

        uint16_t defIndex = item->m_iItemDefinitionIndexC();
        bool isKnife = (defIndex >= 500 && defIndex <= 525) || defIndex == 59;

        if (isKnife && Globals::knife_changer_enabled) {
            apply_knife_skin(weapon,
                Globals::knife_changer_model,
                GetPaintKitId(Globals::knife_changer_paint_kit),
                Globals::knife_changer_wear,
                Globals::knife_changer_seed);
        }
        else if (!isKnife && Globals::skin_changer_enabled) {
            apply_weapon_skin(weapon,
                GetPaintKitId(Globals::skin_changer_paint_kit),
                Globals::skin_changer_wear,
                Globals::skin_changer_seed,
                Globals::skin_changer_custom_name);
        }
    }
}