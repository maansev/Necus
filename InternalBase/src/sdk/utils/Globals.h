#pragma once
#include <cstdint>
#include <Windows.h>

namespace Globals
{
    // ==================== ������� ====================
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
    inline bool grenade_trajectory = false;
    inline bool misc_nightmode = false;
    inline float misc_nightmode_brightness = 0.5f; // 0.0 = �����, 1.0 = ���������


    // ==================== AIMBOT ====================
    inline bool  aimbot_enabled = false;
    inline int   aimbot_key = 0;
    inline bool  aimbot_hitboxes[6] = { true, false, false, false, false, false };
    inline float aimbot_fov = 5.0f;
    inline bool  aimbot_draw_fov = false;
    inline float aimbot_fov_color[4] = { 1.f, 1.f, 1.f, 0.5f };
    inline float aimbot_smooth = 5.0f;
    inline bool  aimbot_silent = false;
    inline bool  aimbot_visibility_check = true;
    inline bool  aimbot_friendly_fire = false;
    inline float aimbot_shot_delay = 0.05f;
    inline float aimbot_kill_delay = 0.20f;
    inline bool  aimbot_lock_target = true;
    inline bool  aimbot_lock_mouse = false;
    inline bool  aimbot_auto_fire = false;
    inline bool  aimbot_auto_scope = false;
    inline bool  aimbot_auto_wall = false;
    inline bool  aimbot_vac_live = false;
    inline int   aimbot_disable_when = 0;
    inline float aimbot_min_damage = 0.f;
    inline float aimbot_hit_chance = 0.f;
    inline int   aimbot_target_hitbox = 0;
    inline int   aimbot_smooth_type = 0;

    // ==================== RCS ====================
    inline bool rcs_enabled = false;
    inline float rcs_x_scale = 1.0f;
    inline float rcs_y_scale = 1.0f;

    // ==================== TRIGGERBOT ====================
    inline bool triggerbot_enabled = false;
    inline int triggerbot_delay = 0;
    inline int triggerbot_hitbox_filter = 0;
    inline int triggerbot_key = 0;   // 0 = always active when enabled

    // ==================== BACKTRACK ====================
    inline bool backtrack_enabled = false;
    inline float backtrack_time = 200.0f;

    // ==================== MISC ====================
    inline bool misc_bhop = false;
    inline int  misc_bhop_mode = 0;   // 0=Rage, 1=Legit
    inline int  misc_bhop_chance = 85;  // Legit mode %
    inline bool  misc_autostrafer = false;
    inline float misc_autostrafer_smooth = 1.0f;
    inline bool misc_infinite_duck = false;
    inline bool misc_noflash = false;
    inline float misc_flash_alpha = 0.f;   // 0=no flash, 255=full flash
    inline bool misc_nosmoke = false;
    inline bool misc_sniper_crosshair = false;
    inline bool misc_reveal_ranks = false;
    inline bool misc_spectator_list = false;
    inline bool misc_radar_2d = false;
    inline bool misc_hitmarker = false;
    inline int misc_hitmarker_sound = 0;
    inline bool misc_damage_logs = false;
    inline bool misc_watermark = true;

    // ==================== MISC EXTENDED ====================
    inline bool  misc_auto_accept = false;
    inline bool  misc_show_money = false;
    inline bool  misc_show_keybind_list = false;  // third Indicators option
    inline bool  misc_thirdperson = false;
    inline int   misc_thirdperson_key = 0;
    inline bool  misc_aspect_ratio_enabled = false;
    inline float misc_aspect_ratio = 1.f;

    // World Modulation
    inline bool  misc_world_mod = false;
    inline float misc_world_color[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float misc_clouds_color[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float misc_sun_color[4] = { 1.f, 0.9f, 0.7f, 1.f };
    inline float misc_sky_color[4] = { 0.5f, 0.7f, 1.f, 1.f };
    inline float misc_particles_color[4] = { 1.f, 1.f, 1.f, 1.f };
    inline bool  misc_override_lighting = false;

    inline bool  misc_spread_circle = false;
    inline bool  misc_penetration_xhair = false;
    inline float misc_pen_no_color[4] = { 1.f, 0.2f, 0.2f, 1.f };
    inline float misc_pen_yes_color[4] = { 0.2f, 1.f, 0.2f, 1.f };
    inline int   misc_removals = 0;   // 0=None, 1=Sniper crosshair, 2=Scope overlay, 3=Both
    inline bool  misc_hit_marker = false;
    inline int   misc_hit_marker_sound = 0;   // 0=None, 1=CoD, 2=Bell
    inline float misc_smoke_alpha = 0.f; // 0=invisible, 255=normal

    // Viewmodel Editor
    inline bool  viewmodel_enabled = false;
    inline float viewmodel_x = 0.f;
    inline float viewmodel_y = 0.f;
    inline float viewmodel_z = 0.f;
    inline float viewmodel_fov = 68.f;  // game default

    // Viewmodel Chams (per slot)
    inline bool  vm_hand_enabled = false;
    inline float vm_hand_vis[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float vm_hand_xqz[4] = { 0.3f, 0.5f, 1.f, 0.5f };
    inline bool  vm_glove_enabled = false;
    inline float vm_glove_vis[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float vm_glove_xqz[4] = { 0.3f, 0.5f, 1.f, 0.5f };
    inline bool  vm_sleeve_enabled = false;
    inline float vm_sleeve_vis[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float vm_sleeve_xqz[4] = { 0.3f, 0.5f, 1.f, 0.5f };
    inline bool  vm_weapon_enabled = false;
    inline float vm_weapon_vis[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float vm_weapon_xqz[4] = { 0.3f, 0.5f, 1.f, 0.5f };

    // ==================== SKINS ====================
    inline bool skin_changer_enabled = false;
    inline int  skin_changer_paint_kit = 1;   // ������ � ������ ��������� ���������
    inline float skin_changer_wear = 0.001f;
    inline int  skin_changer_seed = 661;
    inline char skin_changer_custom_name[161] = "";

    inline bool knife_changer_enabled = false;
    inline int  knife_changer_model = 0;       // ������ � g_item_schema->knives
    inline int  knife_changer_paint_kit = 1;
    inline float knife_changer_wear = 0.001f;
    inline int  knife_changer_seed = 661;

    inline bool glove_changer_enabled = false;
    inline int  glove_changer_model = 0;       // ������ � g_item_schema->gloves
    inline int  glove_changer_paint_kit = 1;
    inline float glove_changer_wear = 0.001f;
    inline int  glove_changer_seed = 661;
}