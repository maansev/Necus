#pragma once
#include "../../sdk/entity/Classes.h"

class c_skin_changer {
public:
    static c_skin_changer& get() { static c_skin_changer inst; return inst; }
    void run(int stage);
private:
    void apply_weapon_skin(C_BasePlayerWeapon* weapon, int paint_kit, float wear, int seed, const char* custom_name);
    void apply_knife_skin(C_BasePlayerWeapon* weapon, int knife_index, int paint_kit, float wear, int seed);
};