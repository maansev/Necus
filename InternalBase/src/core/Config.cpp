#include "Config.h"
#include "../sdk/utils/Globals.h"
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <windows.h>

static std::wstring GetConfigDir()
{
    char dllPath[MAX_PATH];
    GetModuleFileNameA(nullptr, dllPath, MAX_PATH);
    std::string fullPath(dllPath);
    size_t lastSlash = fullPath.find_last_of("\\/");
    std::string dir = fullPath.substr(0, lastSlash + 1) + "configs\\";
    return std::wstring(dir.begin(), dir.end());
}

static std::wstring GetConfigPath(const std::string& name)
{
    return GetConfigDir() + std::wstring(name.begin(), name.end()) + L".nexus";
}

// ������� ������
static void WriteBool(std::ofstream& f, const char* key, bool val) { f << key << "=" << (val ? "1" : "0") << "\n"; }
static void WriteInt(std::ofstream& f, const char* key, int val) { f << key << "=" << val << "\n"; }
static void WriteFloat(std::ofstream& f, const char* key, float val) { f << key << "=" << val << "\n"; }
static void WriteColor(std::ofstream& f, const char* key, float* col) {
    f << key << "=" << col[0] << "," << col[1] << "," << col[2] << "," << col[3] << "\n";
}

// ������ ����� ����-�������� �� �����
static std::vector<std::pair<std::string, std::string>> LoadLines(const std::string& name)
{
    std::vector<std::pair<std::string, std::string>> map;
    std::ifstream file(GetConfigPath(name));
    if (!file.is_open()) return map;

    std::string line;
    while (std::getline(file, line))
    {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        map.emplace_back(key, val);
    }
    return map;
}

static std::string GetVal(const std::vector<std::pair<std::string, std::string>>& map, const char* key, const char* def = "")
{
    for (const auto& [k, v] : map)
        if (k == key) return v;
    return def;
}

static void ReadBool(const std::vector<std::pair<std::string, std::string>>& map, const char* key, bool& out)
{
    auto val = GetVal(map, key, "1");
    out = (val == "1");
}
static void ReadInt(const std::vector<std::pair<std::string, std::string>>& map, const char* key, int& out)
{
    auto val = GetVal(map, key, "0");
    out = std::stoi(val);
}
static void ReadFloat(const std::vector<std::pair<std::string, std::string>>& map, const char* key, float& out)
{
    auto val = GetVal(map, key, "0.0");
    out = std::stof(val);
}
// "Keep" variants: leave the current value untouched when the key is absent
// (older configs) instead of forcing a default.
static void ReadBoolKeep(const std::vector<std::pair<std::string, std::string>>& map, const char* key, bool& out)
{
    auto val = GetVal(map, key, "");
    if (!val.empty()) out = (val == "1");
}
static void ReadFloatKeep(const std::vector<std::pair<std::string, std::string>>& map, const char* key, float& out)
{
    auto val = GetVal(map, key, "");
    if (!val.empty()) out = std::stof(val);
}
static void ReadColor(const std::vector<std::pair<std::string, std::string>>& map, const char* key, float* col)
{
    auto val = GetVal(map, key, "1.0,1.0,1.0,1.0");
    sscanf_s(val.c_str(), "%f,%f,%f,%f", &col[0], &col[1], &col[2], &col[3]);
}

void Config::Load(const std::string& name)
{
    auto map = LoadLines(name);

    // ESP �����
    ReadBool(map, "esp_enabled", Globals::esp_enabled);
    ReadBool(map, "esp_teammates", Globals::esp_teammates);
    ReadFloat(map, "esp_box_thickness", Globals::esp_box_thickness);
    ReadFloat(map, "esp_skeleton_thickness", Globals::esp_skeleton_thickness);

    // Enemy ESP
    ReadBool(map, "enemy_box", Globals::enemy_box);
    ReadInt(map, "enemy_box_style", Globals::enemy_box_style);
    ReadBool(map, "enemy_box_fill", Globals::enemy_box_fill);
    ReadColor(map, "enemy_box_fill_color", Globals::enemy_box_fill_color);
    ReadColor(map, "esp_box_color", Globals::esp_box_color);
    ReadBool(map, "enemy_skeleton", Globals::enemy_skeleton);
    ReadColor(map, "esp_skeleton_color", Globals::esp_skeleton_color);
    ReadBool(map, "enemy_name", Globals::enemy_name);
    ReadColor(map, "esp_name_color", Globals::esp_name_color);
    ReadInt(map, "enemy_name_position", Globals::enemy_name_position);
    ReadBool(map, "enemy_health", Globals::enemy_health);
    ReadInt(map, "enemy_health_position", Globals::enemy_health_position);
    ReadBool(map, "enemy_health_text", Globals::enemy_health_text);
    ReadBool(map, "enemy_armor", Globals::enemy_armor);
    ReadBool(map, "enemy_weapon", Globals::enemy_weapon);
    ReadInt(map, "enemy_weapon_position", Globals::enemy_weapon_position);
    ReadInt(map, "enemy_weapon_display_mode", Globals::enemy_weapon_display_mode);
    ReadBool(map, "enemy_ammo", Globals::enemy_ammo);
    ReadBool(map, "enemy_distance", Globals::enemy_distance);
    ReadBool(map, "enemy_flags", Globals::enemy_flags);
    ReadBool(map, "enemy_glow_enabled", Globals::enemy_glow_enabled);
    ReadInt(map, "enemy_glow_style", Globals::enemy_glow_style);
    ReadFloat(map, "enemy_glow_brightness", Globals::enemy_glow_brightness);
    ReadColor(map, "enemy_glow_color", Globals::enemy_glow_color);
    ReadBool(map, "enemy_glow_skeleton", Globals::enemy_glow_skeleton);
    ReadFloat(map, "enemy_glow_skel_thickness", Globals::enemy_glow_skel_thickness);
    ReadColor(map, "enemy_glow_skel_color", Globals::enemy_glow_skel_color);

    // Team ESP
    ReadBool(map, "team_box", Globals::team_box);
    ReadInt(map, "team_box_style", Globals::team_box_style);
    ReadBool(map, "team_box_fill", Globals::team_box_fill);
    ReadColor(map, "team_box_fill_color", Globals::team_box_fill_color);
    ReadColor(map, "esp_teammate_color", Globals::esp_teammate_color);
    ReadBool(map, "team_skeleton", Globals::team_skeleton);
    ReadColor(map, "team_skeleton_color", Globals::team_skeleton_color);
    ReadBool(map, "team_name", Globals::team_name);
    ReadColor(map, "team_name_color", Globals::team_name_color);
    ReadInt(map, "team_name_position", Globals::team_name_position);
    ReadBool(map, "team_health", Globals::team_health);
    ReadInt(map, "team_health_position", Globals::team_health_position);
    ReadBool(map, "team_health_text", Globals::team_health_text);
    ReadBool(map, "team_armor", Globals::team_armor);
    ReadBool(map, "team_weapon", Globals::team_weapon);
    ReadInt(map, "team_weapon_position", Globals::team_weapon_position);
    ReadInt(map, "team_weapon_display_mode", Globals::team_weapon_display_mode);
    ReadBool(map, "team_ammo", Globals::team_ammo);
    ReadBool(map, "team_distance", Globals::team_distance);
    ReadBool(map, "team_flags", Globals::team_flags);
    ReadBool(map, "team_glow_enabled", Globals::team_glow_enabled);
    ReadInt(map, "team_glow_style", Globals::team_glow_style);
    ReadFloat(map, "team_glow_brightness", Globals::team_glow_brightness);
    ReadColor(map, "team_glow_color", Globals::team_glow_color);
    ReadBool(map, "team_glow_skeleton", Globals::team_glow_skeleton);
    ReadFloat(map, "team_glow_skel_thickness", Globals::team_glow_skel_thickness);
    ReadColor(map, "team_glow_skel_color", Globals::team_glow_skel_color);

    // World
    ReadBool(map, "world_dropped_weapons", Globals::world_dropped_weapons);
    ReadInt(map, "world_dropped_weapons_style", Globals::world_dropped_weapons_style);
    ReadColor(map, "world_weapons_color", Globals::world_weapons_color);
    ReadBool(map, "world_grenades", Globals::world_grenades);
    ReadColor(map, "world_grenades_color", Globals::world_grenades_color);
    ReadBool(map, "world_grenade_radius", Globals::world_grenade_radius);
    ReadBool(map, "world_c4", Globals::world_c4);
    ReadBool(map, "world_c4_timer", Globals::world_c4_timer);

    // Chams
    ReadBool(map, "enemy_chams_enabled", Globals::enemy_chams_enabled);
    ReadColor(map, "enemy_chams_visible_color", Globals::enemy_chams_visible_color);
    ReadColor(map, "enemy_chams_invisible_color", Globals::enemy_chams_invisible_color);
    ReadInt(map, "enemy_chams_material", Globals::enemy_chams_material);
    ReadBool(map, "team_chams_enabled", Globals::team_chams_enabled);
    ReadColor(map, "team_chams_visible_color", Globals::team_chams_visible_color);
    ReadColor(map, "team_chams_invisible_color", Globals::team_chams_invisible_color);
    ReadInt(map, "team_chams_material", Globals::team_chams_material);

    // Aimbot
    ReadBool(map, "aimbot_enabled", Globals::aimbot_enabled);
    ReadInt(map, "aimbot_target_hitbox", Globals::aimbot_target_hitbox);
    ReadFloat(map, "aimbot_fov", Globals::aimbot_fov);
    ReadBool(map, "aimbot_draw_fov", Globals::aimbot_draw_fov);
    ReadFloat(map, "aimbot_smooth", Globals::aimbot_smooth);
    ReadInt(map, "aimbot_smooth_type", Globals::aimbot_smooth_type);
    ReadBool(map, "aimbot_silent", Globals::aimbot_silent);
    ReadBool(map, "aimbot_visibility_check", Globals::aimbot_visibility_check);
    ReadBool(map, "aimbot_friendly_fire", Globals::aimbot_friendly_fire);
    ReadInt(map, "aimbot_key", Globals::aimbot_key);
    if (Globals::aimbot_key == 0) Globals::aimbot_key = VK_LBUTTON; // fallback: never load 0

    // RCS
    ReadBool(map, "rcs_enabled", Globals::rcs_enabled);
    ReadFloat(map, "rcs_x_scale", Globals::rcs_x_scale);
    ReadFloat(map, "rcs_y_scale", Globals::rcs_y_scale);

    // Triggerbot
    ReadBool(map, "triggerbot_enabled", Globals::triggerbot_enabled);
    ReadInt(map, "triggerbot_delay", Globals::triggerbot_delay);
    ReadInt(map, "triggerbot_hitbox_filter", Globals::triggerbot_hitbox_filter);
    ReadInt(map, "triggerbot_key", Globals::triggerbot_key);

    // Backtrack
    ReadBool(map, "backtrack_enabled", Globals::backtrack_enabled);
    ReadFloat(map, "backtrack_time", Globals::backtrack_time);

    // Misc
    ReadBool(map, "misc_bhop", Globals::misc_bhop);
    ReadBool(map, "misc_autostrafer", Globals::misc_autostrafer);
    ReadBool(map, "misc_infinite_duck", Globals::misc_infinite_duck);
    ReadBool(map, "misc_noflash", Globals::misc_noflash);
    ReadFloat(map, "misc_flash_alpha", Globals::misc_flash_alpha);
    ReadBool(map, "misc_nosmoke", Globals::misc_nosmoke);
    ReadBool(map, "misc_sniper_crosshair", Globals::misc_sniper_crosshair);
    ReadBool(map, "misc_reveal_ranks", Globals::misc_reveal_ranks);
    ReadBool(map, "misc_spectator_list", Globals::misc_spectator_list);
    ReadBool(map, "misc_radar_2d", Globals::misc_radar_2d);
    ReadBool(map, "misc_remove_crosshair", Globals::misc_remove_crosshair);
    ReadBool(map, "misc_remove_scope_overlay", Globals::misc_remove_scope_overlay);
    ReadBool(map, "misc_remove_legs", Globals::misc_remove_legs);
    ReadBool(map, "misc_remove_shadows", Globals::misc_remove_shadows);
    ReadBool(map, "misc_hit_marker", Globals::misc_hit_marker);
    ReadBool(map, "misc_hit_marker_tracer_team", Globals::misc_hit_marker_tracer_team);
    ReadBool(map, "misc_hit_marker_tracer_enemy", Globals::misc_hit_marker_tracer_enemy);
    ReadColor(map, "misc_hit_marker_color", Globals::misc_hit_marker_color);
    ReadFloat(map, "misc_hit_marker_size", Globals::misc_hit_marker_size);
    ReadFloat(map, "misc_hit_marker_duration", Globals::misc_hit_marker_duration);
    ReadBool(map, "misc_damage_logs", Globals::misc_damage_logs);
    ReadBool(map, "misc_watermark", Globals::misc_watermark);

    // Misc extended (Keep variants — older configs may lack these keys)
    ReadBoolKeep(map, "misc_auto_accept", Globals::misc_auto_accept);
    ReadBoolKeep(map, "misc_show_keybind_list", Globals::misc_show_keybind_list);
    ReadBoolKeep(map, "misc_thirdperson", Globals::misc_thirdperson);
    ReadFloatKeep(map, "misc_thirdperson_dist", Globals::misc_thirdperson_dist);
    ReadBoolKeep(map, "misc_spread_circle", Globals::misc_spread_circle);
    ReadFloatKeep(map, "misc_autostrafer_smooth", Globals::misc_autostrafer_smooth);

    // Indicator positions (arrays saved as two scalars)
    ReadFloatKeep(map, "ind_watermark_x", Globals::ind_watermark_pos[0]);
    ReadFloatKeep(map, "ind_watermark_y", Globals::ind_watermark_pos[1]);
    ReadFloatKeep(map, "ind_spec_x", Globals::ind_spec_pos[0]);
    ReadFloatKeep(map, "ind_spec_y", Globals::ind_spec_pos[1]);
    ReadFloatKeep(map, "ind_keybind_x", Globals::ind_keybind_pos[0]);
    ReadFloatKeep(map, "ind_keybind_y", Globals::ind_keybind_pos[1]);
    ReadFloatKeep(map, "ind_dmglog_x", Globals::ind_dmglog_pos[0]);
    ReadFloatKeep(map, "ind_dmglog_y", Globals::ind_dmglog_pos[1]);
}

void Config::Save(const std::string& name)
{
    std::wstring dir = GetConfigDir();
    CreateDirectoryW(dir.c_str(), nullptr); // ��������, ��� ����� ����

    std::ofstream f(GetConfigPath(name));
    if (!f.is_open()) return;

    WriteBool(f, "esp_enabled", Globals::esp_enabled);
    WriteBool(f, "esp_teammates", Globals::esp_teammates);
    WriteFloat(f, "esp_box_thickness", Globals::esp_box_thickness);
    WriteFloat(f, "esp_skeleton_thickness", Globals::esp_skeleton_thickness);

    WriteBool(f, "enemy_box", Globals::enemy_box);
    WriteInt(f, "enemy_box_style", Globals::enemy_box_style);
    WriteBool(f, "enemy_box_fill", Globals::enemy_box_fill);
    WriteColor(f, "enemy_box_fill_color", Globals::enemy_box_fill_color);
    WriteColor(f, "esp_box_color", Globals::esp_box_color);
    WriteBool(f, "enemy_skeleton", Globals::enemy_skeleton);
    WriteColor(f, "esp_skeleton_color", Globals::esp_skeleton_color);
    WriteBool(f, "enemy_name", Globals::enemy_name);
    WriteColor(f, "esp_name_color", Globals::esp_name_color);
    WriteInt(f, "enemy_name_position", Globals::enemy_name_position);
    WriteBool(f, "enemy_health", Globals::enemy_health);
    WriteInt(f, "enemy_health_position", Globals::enemy_health_position);
    WriteBool(f, "enemy_health_text", Globals::enemy_health_text);
    WriteBool(f, "enemy_armor", Globals::enemy_armor);
    WriteBool(f, "enemy_weapon", Globals::enemy_weapon);
    WriteInt(f, "enemy_weapon_position", Globals::enemy_weapon_position);
    WriteInt(f, "enemy_weapon_display_mode", Globals::enemy_weapon_display_mode);
    WriteBool(f, "enemy_ammo", Globals::enemy_ammo);
    WriteBool(f, "enemy_distance", Globals::enemy_distance);
    WriteBool(f, "enemy_flags", Globals::enemy_flags);
    WriteBool(f, "enemy_glow_enabled", Globals::enemy_glow_enabled);
    WriteInt(f, "enemy_glow_style", Globals::enemy_glow_style);
    WriteFloat(f, "enemy_glow_brightness", Globals::enemy_glow_brightness);
    WriteColor(f, "enemy_glow_color", Globals::enemy_glow_color);
    WriteBool(f, "enemy_glow_skeleton", Globals::enemy_glow_skeleton);
    WriteFloat(f, "enemy_glow_skel_thickness", Globals::enemy_glow_skel_thickness);
    WriteColor(f, "enemy_glow_skel_color", Globals::enemy_glow_skel_color);

    WriteBool(f, "team_box", Globals::team_box);
    WriteInt(f, "team_box_style", Globals::team_box_style);
    WriteBool(f, "team_box_fill", Globals::team_box_fill);
    WriteColor(f, "team_box_fill_color", Globals::team_box_fill_color);
    WriteColor(f, "esp_teammate_color", Globals::esp_teammate_color);
    WriteBool(f, "team_skeleton", Globals::team_skeleton);
    WriteColor(f, "team_skeleton_color", Globals::team_skeleton_color);
    WriteBool(f, "team_name", Globals::team_name);
    WriteColor(f, "team_name_color", Globals::team_name_color);
    WriteInt(f, "team_name_position", Globals::team_name_position);
    WriteBool(f, "team_health", Globals::team_health);
    WriteInt(f, "team_health_position", Globals::team_health_position);
    WriteBool(f, "team_health_text", Globals::team_health_text);
    WriteBool(f, "team_armor", Globals::team_armor);
    WriteBool(f, "team_weapon", Globals::team_weapon);
    WriteInt(f, "team_weapon_position", Globals::team_weapon_position);
    WriteInt(f, "team_weapon_display_mode", Globals::team_weapon_display_mode);
    WriteBool(f, "team_ammo", Globals::team_ammo);
    WriteBool(f, "team_distance", Globals::team_distance);
    WriteBool(f, "team_flags", Globals::team_flags);
    WriteBool(f, "team_glow_enabled", Globals::team_glow_enabled);
    WriteInt(f, "team_glow_style", Globals::team_glow_style);
    WriteFloat(f, "team_glow_brightness", Globals::team_glow_brightness);
    WriteColor(f, "team_glow_color", Globals::team_glow_color);
    WriteBool(f, "team_glow_skeleton", Globals::team_glow_skeleton);
    WriteFloat(f, "team_glow_skel_thickness", Globals::team_glow_skel_thickness);
    WriteColor(f, "team_glow_skel_color", Globals::team_glow_skel_color);

    WriteBool(f, "world_dropped_weapons", Globals::world_dropped_weapons);
    WriteInt(f, "world_dropped_weapons_style", Globals::world_dropped_weapons_style);
    WriteColor(f, "world_weapons_color", Globals::world_weapons_color);
    WriteBool(f, "world_grenades", Globals::world_grenades);
    WriteColor(f, "world_grenades_color", Globals::world_grenades_color);
    WriteBool(f, "world_grenade_radius", Globals::world_grenade_radius);
    WriteBool(f, "world_c4", Globals::world_c4);
    WriteBool(f, "world_c4_timer", Globals::world_c4_timer);

    WriteBool(f, "enemy_chams_enabled", Globals::enemy_chams_enabled);
    WriteColor(f, "enemy_chams_visible_color", Globals::enemy_chams_visible_color);
    WriteColor(f, "enemy_chams_invisible_color", Globals::enemy_chams_invisible_color);
    WriteInt(f, "enemy_chams_material", Globals::enemy_chams_material);
    WriteBool(f, "team_chams_enabled", Globals::team_chams_enabled);
    WriteColor(f, "team_chams_visible_color", Globals::team_chams_visible_color);
    WriteColor(f, "team_chams_invisible_color", Globals::team_chams_invisible_color);
    WriteInt(f, "team_chams_material", Globals::team_chams_material);

    WriteBool(f, "aimbot_enabled", Globals::aimbot_enabled);
    WriteInt(f, "aimbot_target_hitbox", Globals::aimbot_target_hitbox);
    WriteFloat(f, "aimbot_fov", Globals::aimbot_fov);
    WriteBool(f, "aimbot_draw_fov", Globals::aimbot_draw_fov);
    WriteFloat(f, "aimbot_smooth", Globals::aimbot_smooth);
    WriteInt(f, "aimbot_smooth_type", Globals::aimbot_smooth_type);
    WriteBool(f, "aimbot_silent", Globals::aimbot_silent);
    WriteBool(f, "aimbot_visibility_check", Globals::aimbot_visibility_check);
    WriteBool(f, "aimbot_friendly_fire", Globals::aimbot_friendly_fire);
    WriteInt(f, "aimbot_key", Globals::aimbot_key);

    WriteBool(f, "rcs_enabled", Globals::rcs_enabled);
    WriteFloat(f, "rcs_x_scale", Globals::rcs_x_scale);
    WriteFloat(f, "rcs_y_scale", Globals::rcs_y_scale);

    WriteBool(f, "triggerbot_enabled", Globals::triggerbot_enabled);
    WriteInt(f, "triggerbot_delay", Globals::triggerbot_delay);
    WriteInt(f, "triggerbot_hitbox_filter", Globals::triggerbot_hitbox_filter);
    WriteInt(f, "triggerbot_key", Globals::triggerbot_key);

    WriteBool(f, "backtrack_enabled", Globals::backtrack_enabled);
    WriteFloat(f, "backtrack_time", Globals::backtrack_time);

    WriteBool(f, "misc_bhop", Globals::misc_bhop);
    WriteBool(f, "misc_autostrafer", Globals::misc_autostrafer);
    WriteBool(f, "misc_infinite_duck", Globals::misc_infinite_duck);
    WriteBool(f, "misc_noflash", Globals::misc_noflash);
    WriteFloat(f, "misc_flash_alpha", Globals::misc_flash_alpha);
    WriteBool(f, "misc_nosmoke", Globals::misc_nosmoke);
    WriteBool(f, "misc_sniper_crosshair", Globals::misc_sniper_crosshair);
    WriteBool(f, "misc_reveal_ranks", Globals::misc_reveal_ranks);
    WriteBool(f, "misc_spectator_list", Globals::misc_spectator_list);
    WriteBool(f, "misc_radar_2d", Globals::misc_radar_2d);
    WriteBool(f, "misc_remove_crosshair", Globals::misc_remove_crosshair);
    WriteBool(f, "misc_remove_scope_overlay", Globals::misc_remove_scope_overlay);
    WriteBool(f, "misc_remove_legs", Globals::misc_remove_legs);
    WriteBool(f, "misc_remove_shadows", Globals::misc_remove_shadows);
    WriteBool(f, "misc_hit_marker", Globals::misc_hit_marker);
    WriteBool(f, "misc_hit_marker_tracer_team", Globals::misc_hit_marker_tracer_team);
    WriteBool(f, "misc_hit_marker_tracer_enemy", Globals::misc_hit_marker_tracer_enemy);
    WriteColor(f, "misc_hit_marker_color", Globals::misc_hit_marker_color);
    WriteFloat(f, "misc_hit_marker_size", Globals::misc_hit_marker_size);
    WriteFloat(f, "misc_hit_marker_duration", Globals::misc_hit_marker_duration);
    WriteBool(f, "misc_damage_logs", Globals::misc_damage_logs);
    WriteBool(f, "misc_watermark", Globals::misc_watermark);

    // Misc extended
    WriteBool(f, "misc_auto_accept", Globals::misc_auto_accept);
    WriteBool(f, "misc_show_keybind_list", Globals::misc_show_keybind_list);
    WriteBool(f, "misc_thirdperson", Globals::misc_thirdperson);
    WriteFloat(f, "misc_thirdperson_dist", Globals::misc_thirdperson_dist);
    WriteBool(f, "misc_spread_circle", Globals::misc_spread_circle);
    WriteFloat(f, "misc_autostrafer_smooth", Globals::misc_autostrafer_smooth);

    // Indicator positions (arrays saved as two scalars)
    WriteFloat(f, "ind_watermark_x", Globals::ind_watermark_pos[0]);
    WriteFloat(f, "ind_watermark_y", Globals::ind_watermark_pos[1]);
    WriteFloat(f, "ind_spec_x", Globals::ind_spec_pos[0]);
    WriteFloat(f, "ind_spec_y", Globals::ind_spec_pos[1]);
    WriteFloat(f, "ind_keybind_x", Globals::ind_keybind_pos[0]);
    WriteFloat(f, "ind_keybind_y", Globals::ind_keybind_pos[1]);
    WriteFloat(f, "ind_dmglog_x", Globals::ind_dmglog_pos[0]);
    WriteFloat(f, "ind_dmglog_y", Globals::ind_dmglog_pos[1]);
}

std::vector<std::string> Config::ListConfigs()
{
    std::vector<std::string> names;
    std::wstring dir = GetConfigDir();
    std::wstring search = dir + L"*.nexus";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            std::wstring filename = fd.cFileName;
            size_t dot = filename.find_last_of(L'.');
            if (dot != std::wstring::npos)
            {
                std::string name(filename.begin(), filename.begin() + dot);
                names.push_back(name);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    return names;
}

void Config::DeleteConfig(const std::string& name)
{
    DeleteFileW(GetConfigPath(name).c_str());
}