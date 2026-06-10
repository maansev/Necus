#include "econ_item_attribute_manager.h"
#include "../../sdk/entity/Classes.h"
#include <cstring>
#include <cstdint>

// =============================================================================
//  econ_item_attribute_manager.cpp
//
//  Manages the CEconItemAttribute array that tells CS2 which paint kit / wear /
//  pattern to render on a weapon. Adapted from the working reference project.
//
//  Layout confirmed from working reference (econ_item_attribute_manager.cpp):
//
//      ptr_game_vector_t { uint64_t size; uintptr_t ptr; }
//          lives at C_EconItemView + 0x208 (m_AttributeList) + 0x8 (m_Attributes)
//
//      econ_item_attribute_t layout:
//          char     pad[0x30]
//          uint16_t def_index   // 0x30
//          char     pad2[2]
//          float    value       // 0x34
//          float    init_value  // 0x38
//          int32_t  refundable  // 0x3C
//          bool     set_bonus   // 0x40
//          char     pad3[7]     // → sizeof = 0x48
//
//  Attribute definition indices (CS2):
//      6 = paint_kit_vector_index (float cast of int ID)
//      7 = pattern_seed (float)
//      8 = item_wear   (float)
//
//  MEMORY: Uses a static pool — no CS2 heap needed, no risk of
//          GameFree-on-wrong-heap crash.
// =============================================================================

namespace {
    struct econ_item_attribute_t {
        uint8_t  pad[0x30];
        uint16_t def_index;
        uint8_t  pad2[2];
        float    value;
        float    init_value;
        int32_t  refundable_currency;
        bool     set_bonus;
        uint8_t  pad3[7];
    };
    static_assert(sizeof(econ_item_attribute_t) == 0x48, "attribute size mismatch");

    constexpr int kAttrsPerSlot = 3;
    constexpr int kMaxSlots     = 64;    // max concurrent weapons

    static econ_item_attribute_t s_pool[kMaxSlots * kAttrsPerSlot];
    static int s_next_slot = 0;

    enum : uint16_t {
        ATTR_PAINT_KIT = 6,
        ATTR_PATTERN   = 7,
        ATTR_WEAR      = 8,
    };
}

namespace econ_item_attribute_manager {

void create(C_EconItemView* item, int paint_kit, float wear, int seed)
{
    if (!item || paint_kit <= 0) return;

    auto* attr_list = item->m_attribute_list();
    if (!attr_list) return;

    auto& vec = attr_list->m_attributes();


    // Claim next pool slot (round-robin)
    const int slot = s_next_slot;
    s_next_slot = (s_next_slot + 1) % kMaxSlots;

    econ_item_attribute_t* attrs = s_pool + slot * kAttrsPerSlot;
    memset(attrs, 0, kAttrsPerSlot * sizeof(econ_item_attribute_t));

    attrs[0].def_index  = ATTR_PAINT_KIT;
    attrs[0].value      = static_cast<float>(paint_kit);
    attrs[0].init_value = attrs[0].value;

    attrs[1].def_index  = ATTR_PATTERN;
    attrs[1].value      = static_cast<float>(seed >= 0 ? seed : 0);
    attrs[1].init_value = attrs[1].value;

    attrs[2].def_index  = ATTR_WEAR;
    attrs[2].value      = wear > 0.f ? wear : 0.01f;
    attrs[2].init_value = attrs[2].value;

    vec.size = kAttrsPerSlot;
    vec.ptr  = reinterpret_cast<uintptr_t>(attrs);
}

void remove(C_EconItemView* item)
{
    if (!item) return;

    auto* attr_list = item->m_attribute_list();
    if (!attr_list) return;

    auto& vec = attr_list->m_attributes();
    if (vec.size == 0) return;

    // Check if this is our static pool — if so, just zero and return (no free needed)
    uintptr_t pool_start = reinterpret_cast<uintptr_t>(s_pool);
    uintptr_t pool_end   = pool_start + sizeof(s_pool);

    // Whether ours or game-allocated, we only zero the vector; we never call GameFree
    // (the game won't free it either — only our code touches this vector)
    vec.size = 0;
    vec.ptr  = 0;
}

} // namespace econ_item_attribute_manager
