#pragma once
#include <cstdint>
#include <Windows.h>

namespace Globals
{
    // ==================== БАЗОВЫЕ ====================
    inline float ViewMatrix[16] = { 0.f };
    inline int ScreenWidth = 0;
    inline int ScreenHeight = 0;

    // ==================== ESP ====================
    inline bool esp_enabled = true;
    inline bool esp_teammates = false;
    inline float esp_box_thickness = 1.5f;
    inline float esp_skeleton_thickness = 1.5f;

    // ==================== ENEMY CHAMS ====================
    inline bool enemy_chams_enabled = false;
    inline float enemy_chams_visible_color[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
    inline float enemy_chams_invisible_color[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    inline int enemy_chams_material = 0;

    // ==================== TEAM CHAMS ====================
    inline bool team_chams_enabled = false;
    inline float team_chams_visible_color[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    inline float team_chams_invisible_color[4] = { 0.0f, 0.5f, 1.0f, 1.0f };
    inline int team_chams_material = 0;

    // ==================== ENEMY ESP ====================
    inline bool enemy_box = true;
    inline int enemy_box_style = 0;
    inline bool enemy_box_fill = false;
    inline float enemy_box_fill_color[4] = { 1.0f, 0.0f, 0.0f, 0.2f };
    inline float esp_box_color[4] = { 1.00f, 0.43f, 0.00f, 1.00f };
    inline bool enemy_skeleton = true;
    inline float esp_skeleton_color[4] = { 1.00f, 1.00f, 1.00f, 1.00f };
    inline bool enemy_name = true;
    inline float esp_name_color[4] = { 1.00f, 1.00f, 1.00f, 1.00f };
    inline int enemy_name_position = 0;
    inline bool enemy_health = true;
    inline int enemy_health_position = 2;
    inline bool enemy_health_text = false;
    inline bool enemy_armor = false;
    inline bool enemy_weapon = false;
    inline int enemy_weapon_position = 1;
    inline int enemy_weapon_display_mode = 0;
    inline bool enemy_ammo = false;
    inline bool enemy_distance = false;
    inline bool enemy_flags = false;

    inline bool enemy_glow_enabled = false;
    inline int enemy_glow_style = 3;
    inline float enemy_glow_brightness = 1.2f;
    inline float enemy_glow_color[4] = { 1.00f, 0.43f, 0.00f, 1.00f };
    inline bool enemy_glow_skeleton = false;
    inline float enemy_glow_skel_thickness = 3.0f;
    inline float enemy_glow_skel_color[4] = { 1.00f, 0.00f, 0.00f, 0.80f };

    // ==================== TEAM ESP ====================
    inline bool team_box = false;
    inline int team_box_style = 0;
    inline bool team_box_fill = false;
    inline float team_box_fill_color[4] = { 0.0f, 1.0f, 0.0f, 0.2f };
    inline float esp_teammate_color[4] = { 0.00f, 1.00f, 0.00f, 1.00f };
    inline bool team_skeleton = false;
    inline float team_skeleton_color[4] = { 0.00f, 1.00f, 0.00f, 0.80f };
    inline bool team_name = false;
    inline float team_name_color[4] = { 1.00f, 1.00f, 1.00f, 1.00f };
    inline int team_name_position = 0;
    inline bool team_health = false;
    inline int team_health_position = 2;
    inline bool team_health_text = false;
    inline bool team_armor = false;
    inline bool team_weapon = false;
    inline int team_weapon_position = 1;
    inline int team_weapon_display_mode = 0;
    inline bool team_ammo = false;
    inline bool team_distance = false;
    inline bool team_flags = false;

    inline bool team_glow_enabled = false;
    inline int team_glow_style = 3;
    inline float team_glow_brightness = 1.0f;
    inline float team_glow_color[4] = { 0.00f, 1.00f, 0.00f, 1.00f };
    inline bool team_glow_skeleton = false;
    inline float team_glow_skel_thickness = 3.0f;
    inline float team_glow_skel_color[4] = { 0.00f, 1.00f, 0.00f, 0.75f };

    // ==================== WORLD ====================
    inline bool world_dropped_weapons = false;
    inline int world_dropped_weapons_style = 0;
    inline float world_weapons_color[4] = { 1.00f, 1.00f, 1.00f, 1.00f };
    inline bool world_grenades = false;
    inline float world_grenades_color[4] = { 1.00f, 0.00f, 0.00f, 1.00f };
    inline bool world_grenade_radius = false;
    inline bool world_c4 = false;
    inline bool world_c4_timer = false;
    inline bool misc_nightmode = false;
    inline float misc_nightmode_brightness = 0.5f; // 0.0 = темно, 1.0 = нормально


    // ==================== AIMBOT ====================
    inline bool aimbot_enabled = false;
    inline int aimbot_target_hitbox = 0;
    inline float aimbot_fov = 5.0f;
    inline bool aimbot_draw_fov = false;
    inline float aimbot_smooth = 2.0f;
    inline int aimbot_smooth_type = 0;
    inline bool aimbot_silent = false;
    inline bool aimbot_visibility_check = true;
    inline bool aimbot_friendly_fire = false;
    inline int aimbot_key = VK_LBUTTON;

    // ==================== RCS ====================
    inline bool rcs_enabled = false;
    inline float rcs_x_scale = 1.0f;
    inline float rcs_y_scale = 1.0f;

    // ==================== TRIGGERBOT ====================
    inline bool triggerbot_enabled = false;
    inline int triggerbot_delay = 0;
    inline int triggerbot_hitbox_filter = 0;
    inline int triggerbot_key = VK_LBUTTON;

    // ==================== BACKTRACK ====================
    inline bool backtrack_enabled = false;
    inline float backtrack_time = 200.0f;

    // ==================== MISC ====================
    inline bool misc_bhop = false;
    inline bool misc_autostrafer = false;
    inline bool misc_infinite_duck = false;
    inline bool misc_noflash = false;
    inline float misc_flash_alpha = 255.f;
    inline bool misc_nosmoke = false;
    inline bool misc_sniper_crosshair = false;
    inline bool misc_reveal_ranks = false;
    inline bool misc_spectator_list = false;
    inline bool misc_radar_2d = false;
    inline bool misc_hitmarker = false;
    inline int misc_hitmarker_sound = 0;
    inline bool misc_damage_logs = false;
    inline bool misc_watermark = true;


}