#include "inventory_changer.h"
#include "item_schema.h"
#include "../../sdk/entity/Classes.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/PatternScan.h"
#include "../../sdk/memory/Offsets.h"
#include <Windows.h>

// =============================================================================
//  inventory_changer.cpp  — РЕАЛЬНАЯ реализация на подтверждённых оффсетах
//
//  Цепочка (все оффсеты из cs2-dumper 2026-06-03):
//
//  client.dll + dwLocalPlayerController (0x231F700)
//      → C_CSPlayerController*
//          + 0x810  (m_pInventoryServices) → *ptr → CCSPlayerController_InventoryServices*
//              + 0x40  (m_vecNetworkableLoadout) — CUtlVector inline, заголовок 0x18 байт
//                  elements[i] = NetworkedLoadoutSlot_t (0x10 байт):
//                      + 0x00  pItem  → C_EconItemView*
//                      + 0x08  team   uint16
//                  C_EconItemView:
//                      + 0x1BA  m_iItemDefinitionIndex (uint16)
//                      + 0x280  m_NetworkedDynamicAttributes (CAttributeList inline)
//                          CAttributeList:
//                              + 0x08  m_Attributes (C_UtlVectorEmbeddedNetworkVar)
//                                  elements = CEconItemAttribute[]:
//                                      + 0x30  m_iAttributeDefinitionIndex (uint16)
//                                      + 0x34  m_flValue (float)  ← PAINT KIT / WEAR
//                                  stride = 0x40 байт на элемент
//
//  Атрибут def_index 6  → paint_kit (значение = int ID, хранится как float через bit_cast)
//  Атрибут def_index 8  → item_wear  (значение = float)
// =============================================================================

// ─── bit-cast int ↔ float ────────────────────────────────────────────────────
static inline float int_as_float(int v) {
    float f; memcpy(&f, &v, 4); return f;
}
static inline int float_as_int(float f) {
    int v; memcpy(&v, &f, 4); return v;
}

// ─── Vector layout helpers ───────────────────────────────────────────────────
// CUtlVector<NetworkedLoadoutSlot_t> header (0x18 bytes):
//   +0x00 void* elements
//   +0x08 int   alloc
//   +0x0C int   grow
//   +0x10 int   size
struct CUtlVecHdr {
    void* elements;     // +0
    int   alloc;        // +8
    int   grow;         // +0xC
    int   size;         // +0x10
};

// C_UtlVectorEmbeddedNetworkVar<CEconItemAttribute> header (same layout as CUtlVecHdr):
struct CAttrVecHdr {
    void* elements;     // +0
    int   alloc;        // +8
    int   grow;         // +0xC
    int   size;         // +0x10
};

// NetworkedLoadoutSlot_t (0x10 bytes)
struct LoadoutSlot_t {
    void* pItem;        // +0  → C_EconItemView*
    uint16_t team;      // +8
    uint16_t slot;      // +0xA
    uint32_t _pad;      // +0xC
};

// CEconItemAttribute — raw access by offset
struct AttrRaw {
    uint8_t  _pad[0x30];
    uint16_t def_index;    // +0x30
    uint8_t  _pad2[2];
    float    value;        // +0x34
    float    init_value;   // +0x38
    int32_t  refundable;   // +0x3C
};  // sizeof = 0x40

// ─── Modify one C_EconItemView ────────────────────────────────────────────────
static void apply_to_item(uintptr_t item)
{
    if (!item) return;

    // ── Нож: меняем definition index ──────────────────────────────────────────
    if (Globals::knife_changer_enabled) {
        int mi = Globals::knife_changer_model;
        if (mi >= 0 && mi < (int)knife_items.size())
            *reinterpret_cast<uint16_t*>(item + 0x1BA) = knife_items[mi].definition_index;
    }

    // ── Paint kit / wear через m_NetworkedDynamicAttributes ───────────────────
    if (!Globals::skin_changer_enabled && !Globals::knife_changer_enabled &&
        !Globals::glove_changer_enabled) return;

    uintptr_t attrList = item + 0x280;          // CAttributeList (inline)
    auto* vec = reinterpret_cast<CAttrVecHdr*>(attrList + 0x08); // m_Attributes

    if (!vec->elements) return;
    int count = vec->size;
    if (count <= 0 || count > 32) return;       // защита от мусора

    auto* attrs = reinterpret_cast<AttrRaw*>(vec->elements);

    for (int i = 0; i < count; ++i) {
        auto& a = attrs[i];
        switch (a.def_index) {
        case 6:  // paint_kit_vector_index → int хранится как float
            if (Globals::skin_changer_enabled || Globals::knife_changer_enabled)
                a.value = int_as_float(GetPaintKitId(
                    Globals::knife_changer_enabled
                    ? Globals::knife_changer_paint_kit
                    : Globals::skin_changer_paint_kit));
            break;
        case 8:  // item_wear → float напрямую
            if (Globals::skin_changer_enabled || Globals::knife_changer_enabled)
                a.value = Globals::knife_changer_enabled
                ? Globals::knife_changer_wear
                : Globals::skin_changer_wear;
            break;
        default: break;
        }
    }
}

// ─── Основной апдейт — вызывается каждые ~60 кадров из hkPresent ─────────────
void c_inventory_changer::update()
{
    if (!Globals::skin_changer_enabled &&
        !Globals::knife_changer_enabled &&
        !Globals::glove_changer_enabled)  return;

    __try {
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        // ── Локальный контроллер ────────────────────────────────────────────
        uintptr_t ctrlPtr = *reinterpret_cast<uintptr_t*>(
            client + Offsets::dwLocalPlayerController);
        if (!ctrlPtr) return;

        // ── InventoryServices ───────────────────────────────────────────────
        uintptr_t invSvc = *reinterpret_cast<uintptr_t*>(ctrlPtr + 0x810);
        if (!invSvc) return;

        // ── Networkable loadout vector ──────────────────────────────────────
        auto* vec = reinterpret_cast<CUtlVecHdr*>(invSvc + 0x40);
        if (!vec->elements) return;
        int count = vec->size;
        if (count <= 0 || count > 256) return;  // санитарная проверка

        auto* slots = reinterpret_cast<LoadoutSlot_t*>(vec->elements);

        for (int i = 0; i < count; ++i) {
            uintptr_t item = reinterpret_cast<uintptr_t>(slots[i].pItem);
            if (!item) continue;

            // ── Определяем тип предмета ────────────────────────────────────
            uint16_t defIdx = *reinterpret_cast<uint16_t*>(item + 0x1BA);
            bool isKnife = (defIdx >= 500 && defIdx <= 525) || defIdx == 59;
            bool isGlove = (defIdx >= 5027 && defIdx <= 5035) || defIdx == 4725;

            // ── Пропускаем предметы, которые пользователь не трогал ────────
            if (isKnife && !Globals::knife_changer_enabled)  continue;
            if (isGlove && !Globals::glove_changer_enabled)  continue;
            if (!isKnife && !isGlove && !Globals::skin_changer_enabled) continue;

            apply_to_item(item);
        }

        // ── Перчатки: m_EconGloves в pawn ──────────────────────────────────
        if (Globals::glove_changer_enabled) {
            uintptr_t pawn = *reinterpret_cast<uintptr_t*>(
                client + Offsets::dwLocalPlayerPawn);
            if (pawn) {
                // m_EconGloves — inline C_EconItemView в pawn + 0x1658
                uintptr_t gloveItem = pawn + 0x1658;
                apply_to_item(gloveItem);
                // m_bNeedToReApplyGloves
                *reinterpret_cast<bool*>(pawn + 0x1655) = true;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
