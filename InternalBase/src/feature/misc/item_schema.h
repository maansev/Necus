#pragma once
#include <vector>

struct ItemInfo {
    uint16_t definition_index;
    const char* name;
    const char* model_name;
};

static const std::vector<ItemInfo> knife_items = {
    { 59,  "Default Knife",      "" },
    { 500, "Bayonet",            "models/weapons/v_knife_bayonet.mdl" },
    { 505, "Flip Knife",         "models/weapons/v_knife_flip.mdl" },
    { 506, "Gut Knife",          "models/weapons/v_knife_gut.mdl" },
    { 507, "Karambit",           "models/weapons/v_knife_karam.mdl" },
    { 508, "M9 Bayonet",         "models/weapons/v_knife_m9_bay.mdl" },
    { 509, "Huntsman Knife",     "models/weapons/v_knife_tactical.mdl" },
    { 512, "Falchion Knife",     "models/weapons/v_knife_falchion.mdl" },
    { 514, "Shadow Daggers",     "models/weapons/v_knife_push.mdl" },
    { 515, "Butterfly Knife",    "models/weapons/v_knife_butterfly.mdl" },
    { 516, "Bowie Knife",        "models/weapons/v_knife_survival_bowie.mdl" },
    { 519, "Ursus Knife",        "models/weapons/v_knife_ursus.mdl" },
    { 520, "Navaja Knife",       "models/weapons/v_knife_gypsy_jackknife.mdl" },
    { 521, "Nomad Knife",        "models/weapons/v_knife_outdoor.mdl" },
    { 522, "Stiletto Knife",     "models/weapons/v_knife_stiletto.mdl" },
    { 523, "Talon Knife",        "models/weapons/v_knife_talon.mdl" },
    { 525, "Skeleton Knife",     "models/weapons/v_knife_skeleton.mdl" },
    { 526, "Paracord Knife",     "models/weapons/v_knife_cord.mdl" },
    { 527, "Survival Knife",     "models/weapons/v_knife_canis.mdl" },
};

static const std::vector<ItemInfo> glove_items = {
    { 5029, "Sport Gloves",       "" },
    { 5030, "Driver Gloves",      "" },
    { 5031, "Specialist Gloves",  "" },
    { 5032, "Moto Gloves",        "" },
    { 5033, "Hydra Gloves",       "" },
    { 5034, "Bloodhound Gloves",  "" },
    { 5035, "Hand Wraps",         "" },
};

static const std::vector<ItemInfo> weapon_items = {
    // Rifles
    { 7,  "AK-47",       "" },
    { 8,  "AUG",         "" },
    { 10, "FAMAS",       "" },
    { 13, "Galil AR",    "" },
    { 16, "M4A4",        "" },
    { 60, "M4A1-S",      "" },
    { 38, "SCAR-20",     "" },
    { 39, "SG 553",      "" },
    // Sniper
    { 9,  "AWP",         "" },
    { 40, "SSG 08",      "" },
    // Pistols
    { 1,  "Desert Eagle","" },
    { 4,  "Glock-18",    "" },
    { 30, "Tec-9",       "" },
    { 32, "P2000",       "" },
    { 36, "P250",        "" },
    { 61, "USP-S",       "" },
    { 63, "CZ75 Auto",   "" },
    { 64, "R8 Revolver", "" },
    // SMGs
    { 17, "MAC-10",      "" },
    { 19, "P90",         "" },
    { 23, "MP5-SD",      "" },
    { 24, "UMP-45",      "" },
    { 26, "PP-Bizon",    "" },
    { 33, "MP7",         "" },
    { 34, "MP9",         "" },
    // Heavy
    { 14, "M249",        "" },
    { 25, "XM1014",      "" },
    { 27, "MAG-7",       "" },
    { 28, "Negev",       "" },
    { 29, "Sawed-Off",   "" },
    { 35, "Nova",        "" },
};

struct PaintKitInfo {
    int id;
    const char* name;
};

static const std::vector<PaintKitInfo> paint_kits = {
    // ── Default / no skin ──────────────────────────────────────────────────────
    { 0,   "Default" },

    // ── Knife exclusive / multi-weapon ────────────────────────────────────────
    { 38,  "Fade" },
    { 44,  "Crimson Web" },
    { 51,  "Case Hardened" },
    { 59,  "Slaughter" },
    { 70,  "Doppler Phase 1" },
    { 71,  "Doppler Phase 2" },
    { 72,  "Doppler Phase 3" },
    { 73,  "Doppler Phase 4" },
    { 415, "Doppler Ruby" },
    { 416, "Doppler Sapphire" },
    { 417, "Doppler Black Pearl" },
    { 418, "Doppler Emerald" },
    { 76,  "Lore" },
    { 83,  "Marble Fade" },
    { 117, "Tiger Tooth" },
    { 131, "Ultraviolet" },
    { 143, "Autotronic" },
    { 168, "Freehand" },
    { 180, "Gamma Doppler Phase 1" },
    { 181, "Gamma Doppler Phase 2" },
    { 182, "Gamma Doppler Phase 3" },
    { 183, "Gamma Doppler Phase 4" },
    { 184, "Gamma Doppler Emerald" },
    { 192, "Black Laminate" },
    { 205, "Bright Water" },
    { 222, "Damascus Steel" },
    { 232, "Rust Coat" },
    { 267, "Stained" },
    { 282, "Blue Steel" },
    { 301, "Scorched" },
    { 349, "Night" },

    // ── AK-47 ──────────────────────────────────────────────────────────────────
    { 175, "AK | Redline" },
    { 309, "AK | Vulcan" },
    { 279, "AK | Asiimov" },
    { 490, "AK | Neon Revolution" },
    { 607, "AK | Bloodsport" },
    { 672, "AK | Neon Rider" },
    { 730, "AK | Empress" },
    { 741, "AK | Phantom Disruptor" },
    { 811, "AK | The Empress" },
    { 977, "AK | Nightwish" },
    { 68,  "AK | Fire Serpent" },
    { 321, "AK | Wasteland Rebel" },

    // ── M4A4 ───────────────────────────────────────────────────────────────────
    { 309, "M4A4 | Asiimov" },
    { 447, "M4A4 | Howl" },
    { 657, "M4A4 | Neo-Noir" },
    { 726, "M4A4 | Desolate Space" },
    { 784, "M4A4 | The Emperor" },
    { 830, "M4A4 | In Living Color" },
    { 980, "M4A4 | Eye of Horus" },

    // ── M4A1-S ─────────────────────────────────────────────────────────────────
    { 380, "M4A1-S | Hyper Beast" },
    { 549, "M4A1-S | Mecha Industries" },
    { 631, "M4A1-S | Master Piece" },
    { 670, "M4A1-S | Decimator" },
    { 984, "M4A1-S | Printstream" },
    { 1005,"M4A1-S | Nightmare" },

    // ── AWP ────────────────────────────────────────────────────────────────────
    { 344, "AWP | Asiimov" },
    { 290, "AWP | Hyper Beast" },
    { 291, "AWP | Electric Hive" },
    { 348, "AWP | Wildfire" },
    { 636, "AWP | Neo-Noir" },
    { 742, "AWP | PAW" },
    { 852, "AWP | Gungnir" },
    { 998, "AWP | Printstream" },
    { 344, "AWP | Asiimov" },

    // ── Desert Eagle ───────────────────────────────────────────────────────────
    { 617, "Deagle | Printstream" },
    { 340, "Deagle | Blaze" },
    { 231, "Deagle | Hypnotic" },
    { 471, "Deagle | Kumicho Dragon" },
    { 571, "Deagle | Cobalt Disruption" },
    { 742, "Deagle | Fennec Fox" },

    // ── USP-S ──────────────────────────────────────────────────────────────────
    { 563, "USP-S | Neo-Noir" },
    { 339, "USP-S | Orion" },
    { 597, "USP-S | Cortex" },
    { 880, "USP-S | Printstream" },
    { 338, "USP-S | Kill Confirmed" },

    // ── Glock ──────────────────────────────────────────────────────────────────
    { 676, "Glock | Vogue" },
    { 449, "Glock | Water Elemental" },
    { 412, "Glock | Fade" },
    { 579, "Glock | Neo-Noir" },
    { 720, "Glock | Wasteland Rebel" },

    // ── AUG / SG / FAMAS / Galil ───────────────────────────────────────────────
    { 536, "AUG | Chameleon" },
    { 650, "AUG | Momentum" },
    { 571, "SG 553 | Cyrex" },
    { 650, "FAMAS | Mecha Industries" },
    { 490, "Galil | Cerberus" },

    // ── P90 / MP ───────────────────────────────────────────────────────────────
    { 480, "P90 | Asiimov" },
    { 630, "P90 | Neoqueen" },
    { 680, "MP5-SD | Desert Strike" },

    // ── Generic multi-weapon patterns ─────────────────────────────────────────
    { 7,   "Forest DDPAT" },
    { 12,  "Jungle DDPAT" },
    { 27,  "Anodized" },
    { 28,  "Anodized Gunmetal" },
    { 29,  "Anodized Navy" },
    { 250, "Boreal Forest" },
    { 316, "Urban Masked" },
    { 335, "Safari Mesh" },
};

inline int GetPaintKitId(int index) {
    if (index >= 0 && index < (int)paint_kits.size())
        return paint_kits[index].id;
    return 0;
}