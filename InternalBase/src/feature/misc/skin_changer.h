#pragma once
#include "../../sdk/entity/Classes.h"
#include <vector>

class c_skin_changer {
public:
    static c_skin_changer& get() { static c_skin_changer i; return i; }
    void run(int stage);
    bool should_update = false;

    // Exposed so run_apply() helper can call them without extra indirection
    void process_weapon(C_EconEntity* weapon, C_EconItemView* item, bool force);
    void process_knife(C_EconEntity* weapon, C_EconItemView* item, bool force);

private:
    void apply_skin(C_EconEntity* weapon, C_EconItemView* item,
        int paint_kit, float wear, int seed,
        const char* custom_name, uint16_t def_index);

    uint16_t m_last_knife = 0;
    int      m_last_kit = 0;
    float    m_last_wear = 0.f;
    int      m_last_seed = 0;
    int      m_update_frames = 0;
    float    m_last_spawn = 0.f;
    int      m_last_team = 0;
    std::vector<uint16_t> m_last_weapon_indices;
};
