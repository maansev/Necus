#pragma once
#include <unordered_map>

// Таблица названий оружия (текст)
static const std::unordered_map<int, const char*> weaponNames = {
    { 1,  "Desert Eagle" },
    { 2,  "Dual Berettas" },
    { 3,  "Five-SeveN" },
    { 4,  "Glock-18" },
    { 7,  "AK-47" },
    { 8,  "AUG" },
    { 9,  "AWP" },
    { 10, "FAMAS" },
    { 11, "G3SG1" },
    { 13, "Galil AR" },
    { 14, "M249" },
    { 16, "M4A4" },
    { 17, "MAC-10" },
    { 19, "P90" },
    { 23, "MP5-SD" },
    { 24, "UMP-45" },
    { 25, "XM1014" },
    { 26, "PP-Bizon" },
    { 27, "MAG-7" },
    { 28, "Negev" },
    { 29, "Sawed-Off" },
    { 30, "Tec-9" },
    { 32, "P2000" },
    { 33, "MP7" },
    { 34, "MP9" },
    { 35, "Nova" },
    { 36, "P250" },
    { 38, "SCAR-20" },
    { 39, "SG 553" },
    { 40, "SSG 08" },
    { 42, "Knife" },
    { 59, "Knife" },
    { 60, "M4A1-S" },
    { 61, "USP-S" },
    { 63, "CZ75 Auto" },
    { 64, "R8 Revolver" },
    { 500, "Bayonet" },
    { 505, "Flip Knife" },
    { 506, "Gut Knife" },
    { 507, "Karambit" },
    { 508, "M9 Bayonet" },
    { 509, "Huntsman Knife" },
    { 512, "Falchion Knife" },
    { 514, "Bowie Knife" },
    { 515, "Butterfly Knife" },
    { 516, "Shadow Daggers" },
    { 517, "Paracord Knife" },
    { 518, "Survival Knife" },
    { 519, "Ursus Knife" },
    { 520, "Navaja Knife" },
    { 521, "Nomad Knife" },
    { 522, "Stiletto Knife" },
    { 523, "Talon Knife" },
    { 525, "Skeleton Knife" },
};

// Таблица иконок FontAwesome (используется при esp_weapon_display_mode = 1)
// Символы в UTF-8 без префикса u8, чтобы избежать проблем с типами
// Таблица иконок из csg0_icons.ttf (символ = U+E000 + itemIndex)
static const std::unordered_map<int, const char*> weaponIcons = {
    { 1,  "\xEE\x80\x81" },  // Desert Eagle (0xE001)
    { 2,  "\xEE\x80\x82" },  // Dual Berettas
    { 3,  "\xEE\x80\x83" },  // Five-SeveN
    { 4,  "\xEE\x80\x84" },  // Glock-18
    { 7,  "\xEE\x80\x87" },  // AK-47 (0xE007)
    { 8,  "\xEE\x80\x88" },  // AUG
    { 9,  "\xEE\x80\x89" },  // AWP
    { 10, "\xEE\x80\x8A" },  // FAMAS
    { 11, "\xEE\x80\x8B" },  // G3SG1
    { 13, "\xEE\x80\x8D" },  // Galil AR
    { 14, "\xEE\x80\x8E" },  // M249
    { 16, "\xEE\x80\x90" },  // M4A4
    { 17, "\xEE\x80\x91" },  // MAC-10
    { 19, "\xEE\x80\x93" },  // P90
    { 23, "\xEE\x80\x97" },  // MP5-SD
    { 24, "\xEE\x80\x98" },  // UMP-45
    { 25, "\xEE\x80\x99" },  // XM1014
    { 26, "\xEE\x80\x9A" },  // PP-Bizon
    { 27, "\xEE\x80\x9B" },  // MAG-7
    { 28, "\xEE\x80\x9C" },  // Negev
    { 29, "\xEE\x80\x9D" },  // Sawed-Off
    { 30, "\xEE\x80\x9E" },  // Tec-9
    { 32, "\xEE\x80\xA0" },  // P2000 (0xE020)
    { 33, "\xEE\x80\xA1" },  // MP7
    { 34, "\xEE\x80\xA2" },  // MP9
    { 35, "\xEE\x80\xA3" },  // Nova
    { 36, "\xEE\x80\xA4" },  // P250
    { 38, "\xEE\x80\xA6" },  // SCAR-20
    { 39, "\xEE\x80\xA7" },  // SG 553
    { 40, "\xEE\x80\xA8" },  // SSG 08
    { 42, "\xEE\x80\xAA" },  // Knife (0xE02A)
    { 49, "\xEE\x80\xB1" },  // C4 (0xE031)
    { 59, "\xEE\x80\xBB" },  // Knife
    { 60, "\xEE\x80\xBC" },  // M4A1-S
    { 61, "\xEE\x80\xBD" },  // USP-S
    { 63, "\xEE\x80\xBF" },  // CZ75 Auto (0xE03F)
    { 64, "\xEE\x81\x80" },  // R8 Revolver (0xE040)
    // Ножи (используем общий символ ножа)
    { 500, "\xEE\x80\xAA" }, { 505, "\xEE\x80\xAA" }, { 506, "\xEE\x80\xAA" }, { 507, "\xEE\x80\xAA" },
    { 508, "\xEE\x80\xAA" }, { 509, "\xEE\x80\xAA" }, { 512, "\xEE\x80\xAA" }, { 514, "\xEE\x80\xAA" },
    { 515, "\xEE\x80\xAA" }, { 516, "\xEE\x80\xAA" }, { 517, "\xEE\x80\xAA" }, { 518, "\xEE\x80\xAA" },
    { 519, "\xEE\x80\xAA" }, { 520, "\xEE\x80\xAA" }, { 521, "\xEE\x80\xAA" }, { 522, "\xEE\x80\xAA" },
    { 523, "\xEE\x80\xAA" }, { 525, "\xEE\x80\xAA" },
    // Броня и дефьюз (если понадобятся)
    { 100, "\xEE\x80\xE4" }, // kevlar (0xE064)
    { 101, "\xEE\x80\xE5" }, // kevlar+helmet
    { 102, "\xEE\x80\xE6" }  // defuse kit
};