#pragma once
#include <cstdint>

// =============================================================================
//  inventory_changer.h
//  Применяет скины / нож / перчатки через m_vecNetworkableLoadout.
//  Вызывается из hkFrameStageNotify на stage 6 (как в AndromedaClient).
// =============================================================================

class c_inventory_changer {
public:
    static c_inventory_changer& get() {
        static c_inventory_changer inst;
        return inst;
    }

    void run(int stage);   // вызывать из FSN stage 6

    void invalidate_skins() { m_skins_applied = false; }
    void invalidate_gloves() { m_gloves_applied = false; }
    void invalidate_all() { m_skins_applied = m_gloves_applied = false; }

private:
    void apply_weapon_skins(uintptr_t inv_svc, uintptr_t pawn, uintptr_t entityList);
    void apply_gloves(uintptr_t pawn);

    bool    m_skins_applied = false;
    bool    m_gloves_applied = false;
    float   m_last_spawn_time = -1.f;
};
