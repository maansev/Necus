#pragma once

// =============================================================================
//  inventory_changer.h
//  Меняет paint kit / ножи / перчатки через m_vecNetworkableLoadout.
//  Работает в главном меню и инвентаре без захода в матч.
//  Использует те же Globals::*_changer_* переменные что и skin_changer.
// =============================================================================

class c_inventory_changer {
public:
    static c_inventory_changer& get() {
        static c_inventory_changer inst;
        return inst;
    }

    // Вызывать каждые ~60 кадров из hkPresent
    void update();
};
