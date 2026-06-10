#pragma once
class C_EconItemView;

namespace econ_item_attribute_manager {
    // Call remove() BEFORE create() each time you want to refresh attributes.
    // Uses a static pool — no CS2 heap allocation needed.
    void create(C_EconItemView* item, int paint_kit, float wear, int seed);
    void remove(C_EconItemView* item);
}
