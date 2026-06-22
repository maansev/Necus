#define NOMINMAX
#include "inventory_changer.h"
#include "item_schema.h"
#include "../../sdk/entity/Classes.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/PatternScan.h"
#include "../../sdk/memory/Offsets.h"
#include <Windows.h>
#include <cstring>
#include <algorithm>

// =============================================================================
//  inventory_changer.cpp
//
//  Two-path approach:
//  1. Loadout patching  — patches m_vecNetworkableLoadout (inventory UI)
//  2. Weapon entity patching — patches actual weapon entity fallback fields
//     (this is what makes the skin VISIBLE in-game)
//
//  Weapon entity layout (cs2-dumper 2026-06-03):
//  C_BasePlayerWeapon:
//      + 0x1180  m_AttributeManager  C_AttributeContainer (inline)
//  C_AttributeContainer + 0x50  = m_Item C_EconItemView (inline)
//  C_EconItemView:
//      + 0x1BA  m_iItemDefinitionIndex  uint16
//      + 0x1D0  m_iItemIDHigh           uint32  set 0xFFFFFFFF = use fallback
//      + 0x1D4  m_iItemIDLow            uint32
//  C_BasePlayerWeapon:
//      + 0x1658  m_nFallbackPaintKit  int32
//      + 0x165C  m_nFallbackSeed      int32
//      + 0x1660  m_flFallbackWear     float32
//
//  CCSPlayer_WeaponServices (at pawn + 0x11E0):
//      + 0x20  m_hMyWeapons  CUtlVector (elements*, alloc, grow, size)
//      + 0x60  m_hActiveWeapon  uint32 handle
// =============================================================================

// Loadout offsets
static constexpr ptrdiff_t OFF_INV_SERVICES = 0x810;
static constexpr ptrdiff_t OFF_VEC_LOADOUT = 0x40;

// C_EconItemView relative to its own base
static constexpr ptrdiff_t OFF_EIV_RESTORE_MAT = 0x1B8;
static constexpr ptrdiff_t OFF_EIV_DEF_INDEX = 0x1BA;
static constexpr ptrdiff_t OFF_EIV_ITEM_ID_HIGH = 0x1D0;
static constexpr ptrdiff_t OFF_EIV_ITEM_ID_LOW = 0x1D4;
static constexpr ptrdiff_t OFF_EIV_ACCOUNT_ID = 0x1D8;  // C_EconItemView::m_iAccountID
static constexpr ptrdiff_t OFF_EIV_INITIALIZED = 0x1E8;
static constexpr ptrdiff_t OFF_EIV_DISALLOW_SOC = 0x1E9;
static constexpr ptrdiff_t OFF_EIV_ATTR_LIST = 0x208;
static constexpr ptrdiff_t OFF_EIV_NET_DYN_ATTRS = 0x280;
static constexpr ptrdiff_t OFF_EIV_CUSTOM_NAME = 0x2F8;
static constexpr ptrdiff_t OFF_ATTRLIST_VEC = 0x08;
static constexpr ptrdiff_t ATTR_STRIDE = 0x40;
static constexpr ptrdiff_t ATTR_DEF_INDEX = 0x30;
static constexpr ptrdiff_t ATTR_VALUE = 0x34;

// Pawn gloves
static constexpr ptrdiff_t OFF_PAWN_ECON_GLOVES = 0x1658;

// Weapon entity
static constexpr ptrdiff_t OFF_ATTR_MANAGER = 0x1180;
static constexpr ptrdiff_t OFF_ATTR_CTR_ITEM = 0x50;
static constexpr ptrdiff_t OFF_WPN_FALLBACK_PK = 0x1658;
static constexpr ptrdiff_t OFF_WPN_FALLBACK_SEED = 0x165C;
static constexpr ptrdiff_t OFF_WPN_FALLBACK_WEAR = 0x1660;

// Weapon services in pawn
static constexpr ptrdiff_t OFF_WEAPSVC = 0x11E0;
static constexpr ptrdiff_t OFF_WEAPSVC_WEAPONS = 0x20;

struct UtlVecHdr { void* elements; int alloc; int grow; int size; };

// Local player's SteamID32 — required for fallback skins to take effect:
// the game compares C_EconItemView::m_iAccountID against the owner's account.
// Needed dump string: CBasePlayerController::m_steamID (uint64). Until that
// offset is supplied (Offsets::m_steamID == 0x0), the AccountID write is
// skipped (0 = "don't touch").
static uint32_t s_local_account_id = 0;

static inline float int_as_float(int v) { float f; memcpy(&f, &v, 4); return f; }

static inline bool is_knife_defidx(uint16_t d) {
    return (d >= 500 && d <= 527) || d == 59;
}
static inline bool is_glove_defidx(uint16_t d) {
    return (d >= 5027 && d <= 5035) || d == 4725;
}

// Resolve entity handle through entity list
static uintptr_t ResolveHandle(uintptr_t entityList, uint32_t handle) {
    if (!handle || handle == 0xFFFFFFFF) return 0;
    uintptr_t listEntry = *reinterpret_cast<uintptr_t*>(
        entityList + 8 * ((handle & 0x7FFF) >> 9) + 16);
    if (!listEntry) return 0;
    return *reinterpret_cast<uintptr_t*>(listEntry + 112 * (handle & 0x1FF));
}

// Write paint_kit/wear/seed into existing attribute slots (loadout items)
static void write_attrs(uintptr_t item, int paint_kit, float wear, int seed) {
    for (int list = 0; list < 2; ++list) {
        ptrdiff_t list_off = (list == 0) ? OFF_EIV_ATTR_LIST : OFF_EIV_NET_DYN_ATTRS;
        auto* vec = reinterpret_cast<UtlVecHdr*>(item + list_off + OFF_ATTRLIST_VEC);
        if (!vec || !vec->elements) continue;
        int count = vec->size;
        if (count <= 0 || count > 64) continue;
        auto* base = reinterpret_cast<uint8_t*>(vec->elements);
        for (int i = 0; i < count; ++i) {
            auto* attr = base + i * ATTR_STRIDE;
            uint16_t def = *reinterpret_cast<uint16_t*>(attr + ATTR_DEF_INDEX);
            float* val = reinterpret_cast<float*>(attr + ATTR_VALUE);
            if (def == 6 && paint_kit > 0) *val = int_as_float(paint_kit);
            else if (def == 8 && wear > 0.f)    *val = wear;
            else if (def == 9 && seed > 0)      *val = int_as_float(seed);
        }
    }
}

static void patch_loadout_item(uintptr_t item, uint16_t new_def, int paint_kit,
    float wear, int seed, const char* custom_name)
{
    if (!item) return;
    *reinterpret_cast<bool*>    (item + OFF_EIV_INITIALIZED) = true;
    *reinterpret_cast<bool*>    (item + OFF_EIV_DISALLOW_SOC) = false;
    *reinterpret_cast<bool*>    (item + OFF_EIV_RESTORE_MAT) = true;
    if (new_def != 0)
        *reinterpret_cast<uint16_t*>(item + OFF_EIV_DEF_INDEX) = new_def;
    // m_iItemIDHigh = -1 forces the engine to read the Fallback* fields
    *reinterpret_cast<uint32_t*>(item + OFF_EIV_ITEM_ID_HIGH) = 0xFFFFFFFF;
    *reinterpret_cast<uint32_t*>(item + OFF_EIV_ITEM_ID_LOW) = 0x00000001;
    if (s_local_account_id)
        *reinterpret_cast<uint32_t*>(item + OFF_EIV_ACCOUNT_ID) = s_local_account_id;
    write_attrs(item, paint_kit, wear, seed);
    if (custom_name && custom_name[0])
        strncpy_s(reinterpret_cast<char*>(item + OFF_EIV_CUSTOM_NAME), 161, custom_name, 160);
}

// Patch actual weapon entity fallback fields — makes the skin render in-game
static void patch_weapon_entity(uintptr_t weapon, uint16_t new_def,
    int paint_kit, float wear, int seed)
{
    if (!weapon) return;
    uintptr_t econItem = weapon + OFF_ATTR_MANAGER + OFF_ATTR_CTR_ITEM;

    // m_iItemIDHigh = -1 forces the engine to read the Fallback* fields
    *reinterpret_cast<uint32_t*>(econItem + OFF_EIV_ITEM_ID_HIGH) = 0xFFFFFFFF;
    *reinterpret_cast<uint32_t*>(econItem + OFF_EIV_ITEM_ID_LOW) = 0x00000001;
    if (s_local_account_id)
        *reinterpret_cast<uint32_t*>(econItem + OFF_EIV_ACCOUNT_ID) = s_local_account_id;
    *reinterpret_cast<bool*>    (econItem + OFF_EIV_INITIALIZED) = true;
    *reinterpret_cast<bool*>    (econItem + OFF_EIV_DISALLOW_SOC) = false;

    if (new_def != 0)
        *reinterpret_cast<uint16_t*>(econItem + OFF_EIV_DEF_INDEX) = new_def;

    if (paint_kit > 0)
        *reinterpret_cast<int32_t*>(weapon + OFF_WPN_FALLBACK_PK) = paint_kit;
    if (seed > 0)
        *reinterpret_cast<int32_t*>(weapon + OFF_WPN_FALLBACK_SEED) = seed;
    if (wear > 0.f)
        *reinterpret_cast<float*>  (weapon + OFF_WPN_FALLBACK_WEAR) = wear;
}

// ── apply_weapon_skins ────────────────────────────────────────────────────────
void c_inventory_changer::apply_weapon_skins(uintptr_t inv_svc, uintptr_t pawn,
    uintptr_t entityList)
{
    // Part 1: loadout patching (inventory display)
    if (inv_svc) {
        auto* vec = reinterpret_cast<UtlVecHdr*>(inv_svc + OFF_VEC_LOADOUT);
        if (vec && vec->elements && vec->size > 0 && vec->size <= 256) {
            auto* slots = reinterpret_cast<uint8_t*>(vec->elements);
            for (int i = 0; i < vec->size; ++i) {
                uintptr_t item = *reinterpret_cast<uintptr_t*>(slots + i * 0x10);
                if (!item) continue;
                uint16_t def = *reinterpret_cast<uint16_t*>(item + OFF_EIV_DEF_INDEX);
                bool knife = is_knife_defidx(def);
                bool glove = is_glove_defidx(def);
                if (glove) continue;
                if (knife && !Globals::knife_changer_enabled) continue;
                if (!knife && !Globals::skin_changer_enabled) continue;

                int pk = 0; float wr = 0.f; int sd = 0; uint16_t nd = 0;
                if (knife) {
                    int mi = Globals::knife_changer_model;
                    if (mi >= 0 && mi < (int)knife_items.size())
                        nd = knife_items[mi].definition_index;
                    pk = GetPaintKitId(Globals::knife_changer_paint_kit);
                    wr = Globals::knife_changer_wear;
                    sd = Globals::knife_changer_seed;
                }
                else {
                    pk = GetPaintKitId(Globals::skin_changer_paint_kit);
                    wr = Globals::skin_changer_wear;
                    sd = Globals::skin_changer_seed;
                }
                patch_loadout_item(item, nd, pk, wr, sd, Globals::skin_changer_custom_name);
            }
        }
    }

    // Part 2: weapon entity patching (makes skins render visually)
    if (!pawn || !entityList) return;

    uintptr_t weapSvcPtr = *reinterpret_cast<uintptr_t*>(pawn + OFF_WEAPSVC);
    if (!weapSvcPtr) return;

    auto* weapVec = reinterpret_cast<UtlVecHdr*>(weapSvcPtr + OFF_WEAPSVC_WEAPONS);
    if (!weapVec || !weapVec->elements || weapVec->size <= 0 || weapVec->size > 64) return;

    auto* handles = reinterpret_cast<uint32_t*>(weapVec->elements);
    for (int i = 0; i < weapVec->size; ++i) {
        uintptr_t weapon = ResolveHandle(entityList, handles[i]);
        if (!weapon) continue;

        uintptr_t econItem = weapon + OFF_ATTR_MANAGER + OFF_ATTR_CTR_ITEM;
        uint16_t  def = *reinterpret_cast<uint16_t*>(econItem + OFF_EIV_DEF_INDEX);

        bool knife = is_knife_defidx(def);
        bool glove = is_glove_defidx(def);
        if (glove) continue;
        if (knife && !Globals::knife_changer_enabled) continue;
        if (!knife && !Globals::skin_changer_enabled) continue;

        int pk = 0; float wr = 0.f; int sd = 0; uint16_t nd = 0;
        if (knife) {
            int mi = Globals::knife_changer_model;
            if (mi >= 0 && mi < (int)knife_items.size())
                nd = knife_items[mi].definition_index;
            pk = GetPaintKitId(Globals::knife_changer_paint_kit);
            wr = Globals::knife_changer_wear;
            sd = Globals::knife_changer_seed;
        }
        else {
            pk = GetPaintKitId(Globals::skin_changer_paint_kit);
            wr = Globals::skin_changer_wear;
            sd = Globals::skin_changer_seed;
        }
        patch_weapon_entity(weapon, nd, pk, wr, sd);
    }

    m_skins_applied = true;
}

// ── apply_gloves ──────────────────────────────────────────────────────────────
void c_inventory_changer::apply_gloves(uintptr_t pawn)
{
    if (!pawn || !Globals::glove_changer_enabled) return;

    uintptr_t glove_item = pawn + OFF_PAWN_ECON_GLOVES;
    int mi = Globals::glove_changer_model;
    uint16_t new_def = 0;
    if (mi >= 0 && mi < (int)glove_items.size())
        new_def = glove_items[mi].definition_index;

    int   paint_kit = GetPaintKitId(Globals::glove_changer_paint_kit);
    float wear = Globals::glove_changer_wear;
    int   seed = Globals::glove_changer_seed;

    patch_loadout_item(glove_item, new_def, paint_kit, wear, seed, nullptr);
    *reinterpret_cast<bool*>(pawn + 0x1655) = true; // m_bNeedToReApplyGloves

    m_gloves_applied = true;
}

// ── run ───────────────────────────────────────────────────────────────────────
void c_inventory_changer::run(int stage)
{
    if (stage != 6) return;

    bool need_skins = Globals::skin_changer_enabled || Globals::knife_changer_enabled;
    bool need_gloves = Globals::glove_changer_enabled;
    if (!need_skins && !need_gloves) { m_skins_applied = m_gloves_applied = false; return; }

    __try {
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        uintptr_t entityList = *reinterpret_cast<uintptr_t*>(client + Offsets::dwEntityList);
        uintptr_t ctrl = *reinterpret_cast<uintptr_t*>(client + Offsets::dwLocalPlayerController);
        if (!ctrl) return;

        uintptr_t inv_svc = *reinterpret_cast<uintptr_t*>(ctrl + OFF_INV_SERVICES);
        uintptr_t pawn = *reinterpret_cast<uintptr_t*>(client + Offsets::dwLocalPlayerPawn);

        // SteamID64 -> SteamID32 for m_iAccountID (skipped while offset is 0x0)
        if (Offsets::m_steamID != 0) {
            uint64_t sid64 = *reinterpret_cast<uint64_t*>(ctrl + Offsets::m_steamID);
            if (sid64 > 76561197960265728ull)
                s_local_account_id = static_cast<uint32_t>(sid64 - 76561197960265728ull);
        }

        static uintptr_t s_last_pawn = 0;
        if (pawn != s_last_pawn) {
            m_skins_applied = m_gloves_applied = false;
            s_last_pawn = pawn;
        }

        // Apply every frame so newly picked weapons get skinned immediately
        if (need_skins)
            apply_weapon_skins(inv_svc, pawn, entityList);

        if (need_gloves && pawn)
            apply_gloves(pawn);

    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        m_skins_applied = m_gloves_applied = false;
    }
}
