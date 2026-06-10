#include "skin_changer.h"
#include "econ_item_attribute_manager.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/memory/Offsets.h"
#include "../../sdk/memory/PatternScan.h"
#include "../../sdk/utils/Globals.h"
#include "../misc/item_schema.h"
#include <cstring>
#include <vector>
#include <Windows.h>

void c_skin_changer::apply_skin(C_EconEntity* weapon, C_EconItemView* item,
    int paint_kit, float wear, int seed,
    const char* custom_name, uint16_t def_index)
{
    if (!weapon || !item) return;

    item->m_iItemIDHigh() = 0xFFFFFFFF;
    item->m_iItemIDLow() = 0xFFFFFFFF;
    item->m_bInitialized() = true;
    item->m_bDisallowSOC() = false;
    item->m_bRestoreCustomMaterialAfterPrecache() = true;

    weapon->m_nFallbackPaintKit() = paint_kit;
    weapon->m_flFallbackWear() = (wear > 0.f && wear <= 1.f) ? wear : 0.01f;
    weapon->m_nFallbackSeed() = seed;

    econ_item_attribute_manager::remove(item);
    econ_item_attribute_manager::create(item, paint_kit, wear, seed);

    if (custom_name && custom_name[0])
        strncpy_s(item->m_szCustomName(), 161, custom_name, 160);
    else
        item->m_szCustomName()[0] = '\0';
}

void c_skin_changer::process_weapon(C_EconEntity* weapon, C_EconItemView* item, bool force)
{
    if (!Globals::skin_changer_enabled) return;

    int kit = GetPaintKitId(Globals::skin_changer_paint_kit);
    if (kit == 0) return;

    if (!force && weapon->m_nFallbackPaintKit() == kit
        && weapon->m_flFallbackWear() == Globals::skin_changer_wear)
        return;

    apply_skin(weapon, item, kit,
        Globals::skin_changer_wear,
        Globals::skin_changer_seed,
        Globals::skin_changer_custom_name,
        item->m_iItemDefinitionIndexC());
}

void c_skin_changer::process_knife(C_EconEntity* weapon, C_EconItemView* item, bool force)
{
    if (!Globals::knife_changer_enabled) return;

    int mi = Globals::knife_changer_model;
    if (mi <= 0 || mi >= (int)knife_items.size()) return;

    const uint16_t selected = knife_items[mi].definition_index;
    if (!selected) return;

    int kit = GetPaintKitId(Globals::knife_changer_paint_kit);

    const bool config_changed = (m_last_knife != selected)
        || (m_last_kit != kit)
        || (m_last_wear != Globals::knife_changer_wear)
        || (m_last_seed != Globals::knife_changer_seed);

    if (!force && !config_changed && item->m_iItemDefinitionIndexC() == selected)
        return;

    item->m_iItemDefinitionIndex() = selected;
    item->m_iEntityQuality() = 3;

    const char* mdl = knife_items[mi].model_name;
    if (mdl && mdl[0])
        weapon->set_model(mdl);

    apply_skin(weapon, item, kit,
        Globals::knife_changer_wear,
        Globals::knife_changer_seed,
        "",
        selected);

    m_last_knife = selected;
    m_last_kit = kit;
    m_last_wear = Globals::knife_changer_wear;
    m_last_seed = Globals::knife_changer_seed;
}

void c_skin_changer::run_inner()
{
    const bool skin_en = Globals::skin_changer_enabled;
    const bool knife_en = Globals::knife_changer_enabled;
    if (!skin_en && !knife_en) return;

    auto* local_pawn = EntityManager::Get().GetLocalPawn();
    if (!local_pawn || !local_pawn->IsAlive()) return;

    uintptr_t weapSvc = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(local_pawn) + Offsets::m_pWeaponServices);
    if (!weapSvc) return;

    // m_hMyWeapons = 0x48 (подтверждено cs2-dumper 2026-06-03)
    auto* myWeapons = reinterpret_cast<CUtlVectorSimple*>(weapSvc + 0x48);
    int count = myWeapons->m_size;
    if (count <= 0 || count > 64 || !myWeapons->m_elements) return;

    auto* handles = reinterpret_cast<CHandleSimple*>(myWeapons->m_elements);

    for (int i = 0; i < count; ++i) {
        uint32_t handle = handles[i].m_handle;
        if (!handle || handle == 0xFFFFFFFF) continue;

        uintptr_t weapPtr = EntityManager::Get().GetEntityFromHandle(handle);
        if (!weapPtr) continue;

        auto* weapon = reinterpret_cast<C_EconEntity*>(weapPtr);
        auto* attr = weapon->GetAttrContainer();
        if (!attr) continue;
        auto* item = attr->GetItemPtr();
        if (!item) continue;

        uint16_t def = item->m_iItemDefinitionIndexC();
        bool is_knife = (def == 42 || def == 59 || (def >= 500 && def <= 527));

        if (is_knife && knife_en)
            process_knife(weapon, item, false);
        else if (!is_knife && skin_en)
            process_weapon(weapon, item, false);
    }
}

void c_skin_changer::run(int stage)
{
    if (stage != 7) return;
    __try {
        run_inner();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}