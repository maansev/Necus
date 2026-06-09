#include "skin_changer.h"
#include "econ_item_attribute_manager.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/memory/Offsets.h"
#include "../../sdk/memory/PatternScan.h"
#include "../../sdk/utils/Globals.h"
#include "../misc/item_schema.h"       // GetPaintKitId, knife_items, glove_items
#include <cstring>
#include <vector>
#include <Windows.h>

// =============================================================================
//  skin_changer.cpp  —  Necus project
//
//  Adapted from working reference project. Key differences vs old Necus version:
//   • stage == 7   (NOT 3 — CS2 FrameStageNotify stage 7 is the correct hook)
//   • m_iItemIDHigh = 0xFFFFFFFF (forces fallback / override path in CS2)
//   • m_bRestoreCustomMaterialAfterPrecache = true
//   • econ_item_attribute_manager::create()  ← NEEDED for skin to actually render
//   • update_composite() / update_skin() / update_weapon_data() VMT calls
//   • For knives: update_subclass() to swap model + skin
//
//  VMT indices confirmed from working reference (c_cs_player_pawn.hpp):
//      update_composite   = 10
//      update_skin        = 110
//      update_weapon_data = 195
// =============================================================================

void c_skin_changer::apply_skin(C_EconEntity* weapon, C_EconItemView* item,
    int paint_kit, float wear, int seed,
    const char* custom_name, uint16_t def_index)
{
    if (!weapon || !item) return;

    // 1. Remove old attributes so create() can add fresh ones
    econ_item_attribute_manager::remove(item);

    // 2. Mark item as "override" — forces CS2 to use fallback fields
    item->m_iItemIDHigh() = 0xFFFFFFFF;
    item->m_bInitialized() = true;
    item->m_bDisallowSOC() = false;
    item->m_bRestoreCustomMaterialAfterPrecache() = true;

    // 3. Set fallback paint fields directly on weapon entity
    weapon->m_nFallbackPaintKit() = paint_kit;
    weapon->m_flFallbackWear() = wear;
    weapon->m_nFallbackSeed() = seed;

    // 4. Build attribute list (paint_kit/pattern/wear attributes)
    econ_item_attribute_manager::create(item, paint_kit, wear, seed);

    // 5. Optional: custom nametag
    if (custom_name && custom_name[0] != '\0')
        strncpy_s(item->m_szCustomName(), 161, custom_name, 160);
    else
        item->m_szCustomName()[0] = '\0';

    // 6. Mesh group mask: 1 = new-style skin, 2 = old-style  (default safe: 1)
    if (auto* node = weapon->m_scene_node())
        node->set_mesh_group_mask(1);

    // 7. Notify engine to re-read and re-render
    weapon->update_composite(true);
    weapon->update_skin(true);
    weapon->update_weapon_data();
}

void c_skin_changer::process_weapon(C_EconEntity* weapon, C_EconItemView* item, bool force)
{
    if (!Globals::skin_changer_enabled) return;

    int kit = GetPaintKitId(Globals::skin_changer_paint_kit);
    if (kit == 0) return;

    if (!force && weapon->m_nFallbackPaintKit() == kit) return;

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

    // Swap knife type
    item->m_iItemDefinitionIndex() = selected;
    item->m_iEntityQuality() = 3;  // QUALITY_UNUSUAL

    // update_subclass changes internal type so the correct 3D model loads
    weapon->update_subclass(selected);

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

void c_skin_changer::run(int stage)
{
    // CS2 FrameStageNotify stage 7 — fires after network data update, before render
    if (stage != 7) return;

    const bool skin_en = Globals::skin_changer_enabled;
    const bool knife_en = Globals::knife_changer_enabled;
    if (!skin_en && !knife_en) return;

    __try {
        auto* local_pawn = EntityManager::Get().GetLocalPawn();
        if (!local_pawn || !local_pawn->IsAlive()) return;

        // Get weapon services
        uintptr_t weapSvc = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(local_pawn) + Offsets::m_pWeaponServices);
        if (!weapSvc) return;

        // m_hMyWeapons at +0x48 within CPlayer_WeaponServices
        auto* myWeapons = reinterpret_cast<CUtlVectorSimple*>(weapSvc + 0x48);
        int count = myWeapons->m_size;
        if (count <= 0 || count > 64 || !myWeapons->m_elements) return;

        auto* handles = reinterpret_cast<CHandleSimple*>(myWeapons->m_elements);

        // ── Change-detection ────────────────────────────────────────────────
        std::vector<uint16_t> cur_indices;
        cur_indices.reserve(count);
        for (int i = 0; i < count; ++i) {
            uintptr_t ptr = EntityManager::Get().GetEntityFromHandle(handles[i].m_handle);
            if (!ptr) continue;
            auto* itm = reinterpret_cast<C_EconEntity*>(ptr)->GetAttrContainer()->GetItemPtr();
            if (itm) cur_indices.push_back(itm->m_iItemDefinitionIndexC());
        }
        bool weapons_changed = (cur_indices != m_last_weapon_indices);
        m_last_weapon_indices = cur_indices;

        if (weapons_changed || should_update)
            m_update_frames = 6;
        should_update = false;

        if (m_update_frames <= 0) return;

        // ── Apply to each weapon ─────────────────────────────────────────────
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
            bool is_knife = (def == 42 || def == 59 || (def >= 500 && def <= 526));

            if (is_knife && knife_en)
                process_knife(weapon, item, true);
            else if (!is_knife && skin_en)
                process_weapon(weapon, item, true);
        }

        m_update_frames--;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
