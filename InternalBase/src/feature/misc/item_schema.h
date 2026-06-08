#pragma once
#include <vector>

struct ItemInfo {
    uint16_t definition_index;
    const char* name;
    const char* model_name;
};

// Ножи
static const std::vector<ItemInfo> knife_items = {
    { 59,  "Default Knife", "" },
    { 500, "Bayonet", "models/weapons/v_knife_bayonet.mdl" },
    { 505, "Flip Knife", "models/weapons/v_knife_flip.mdl" },
    { 506, "Gut Knife", "models/weapons/v_knife_gut.mdl" },
    { 507, "Karambit", "models/weapons/v_knife_karam.mdl" },
    { 508, "M9 Bayonet", "models/weapons/v_knife_m9_bay.mdl" },
    { 509, "Huntsman Knife", "models/weapons/v_knife_tactical.mdl" },
    { 512, "Falchion Knife", "models/weapons/v_knife_falchion.mdl" },
    { 515, "Butterfly Knife", "models/weapons/v_knife_butterfly.mdl" },
    { 522, "Stiletto Knife", "models/weapons/v_knife_stiletto.mdl" },
    { 523, "Talon Knife", "models/weapons/v_knife_talon.mdl" },
    { 525, "Skeleton Knife", "models/weapons/v_knife_skeleton.mdl" }
};

// Перчатки
static const std::vector<ItemInfo> glove_items = {
    { 5029, "Sport Gloves", "" },
    { 5030, "Driver Gloves", "" },
    { 5031, "Specialist Gloves", "" },
    { 5032, "Moto Gloves", "" },
    { 5033, "Hydra Gloves", "" },
    { 5034, "Bloodhound Gloves", "" }
};

// Оружие (для меню)
static const std::vector<ItemInfo> weapon_items = {
    { 7,  "AK-47", "" },
    { 16, "M4A4", "" },
    { 60, "M4A1-S", "" },
    { 9,  "AWP", "" },
    { 61, "USP-S", "" },
    { 4,  "Glock-18", "" },
    { 1,  "Desert Eagle", "" },
    { 8,  "AUG", "" },
    { 10, "FAMAS", "" },
    { 13, "Galil AR", "" },
    { 14, "M249", "" },
    { 17, "MAC-10", "" },
    { 19, "P90", "" },
    { 23, "MP5-SD", "" },
    { 24, "UMP-45", "" },
    { 25, "XM1014", "" },
    { 26, "PP-Bizon", "" },
    { 27, "MAG-7", "" },
    { 28, "Negev", "" },
    { 29, "Sawed-Off", "" },
    { 30, "Tec-9", "" },
    { 32, "P2000", "" },
    { 33, "MP7", "" },
    { 34, "MP9", "" },
    { 35, "Nova", "" },
    { 36, "P250", "" },
    { 38, "SCAR-20", "" },
    { 39, "SG 553", "" },
    { 40, "SSG 08", "" },
    { 63, "CZ75 Auto", "" },
    { 64, "R8 Revolver", "" }
};

// Раскраски
struct PaintKitInfo {
    int id;
    const char* name;
};

static const std::vector<PaintKitInfo> paint_kits = {
    { 0,   "Default" },
    { 38,  "Fade" },
    { 44,  "Crimson Web" },
    { 51,  "Case Hardened" },
    { 59,  "Slaughter" },
    { 70,  "Doppler (Phase 1)" },
    { 76,  "Lore" },
    { 83,  "Marble Fade" },
    { 117, "Tiger Tooth" },
    { 131, "Ultraviolet" },
    { 143, "Autotronic" },
    { 168, "Freehand" },
    { 180, "Gamma Doppler" },
    { 192, "Black Laminate" },
    { 205, "Bright Water" },
    { 222, "Damascus Steel" },
    { 232, "Rust Coat" },
    { 250, "Boreal Forest" },
    { 267, "Stained" },
    { 282, "Blue Steel" },
    { 301, "Scorched" },
    { 316, "Urban Masked" },
    { 335, "Safari Mesh" },
    { 349, "Night" }
};

inline int GetPaintKitId(int index) {
    if (index >= 0 && index < (int)paint_kits.size())
        return paint_kits[index].id;
    return 0;
}