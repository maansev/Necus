// =============================================================================
//  Menu.cpp  —  Necus  |  Dear ImGui implementation
//
//  Re-skinned to match the "gamesense v2" visual language:
//    Window  : 760 x 600   |  Sidebar 66px  |  Header 54px
//    Theme   : bg #1A1A1A  card #1D1D1D  header #131313  border #2A2A2A
//    Accent  : dusty mauve #C195A0
//    Cards   : centered title + accent underline, custom widgets
//
//  - All EXISTING Globals:: features are preserved (Legit, Visuals, Misc, Config).
//  - NEW features ported from the mock live in namespace NecusState below:
//      * Rage tab  : per-weapon Aimbot (General + Accuracy) + global Anti-Aim
//      * Header    : per-weapon picker ("Global" -> every CS2 weapon)
//      * Keybinds  : right-aligned chips on toggles (Win32 VK capture)
//      * Skins UI  : knife / gloves / paint controls
//    Migrate NecusState into Globals/Config when you wire the backend.
// =============================================================================

#include "Menu.h"
#include "../../ext/imgui/imgui.h"
#include "../../ext/imgui/imgui_internal.h"
#include "../sdk/utils/Globals.h"
#include "../sdk/memory/Offsets.h"
#include "../sdk/memory/PatternScan.h"
#include "../core/Config.h"
#include "PoppinsFont.h"   // embedded Poppins TTF (gamesense v2 reference typeface)
#include "../feature/misc/item_schema.h"
#include "../feature/visuals/Visuals.h"   // damage log accessor
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#include <Windows.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  PALETTE  (matches the gamesense v2 CSS variables)
// ─────────────────────────────────────────────────────────────────────────────
namespace Col {
    static const ImVec4 Window = ImVec4(0.102f, 0.102f, 0.102f, 1.00f); // #1A1A1A
    static const ImVec4 Sidebar = ImVec4(0.086f, 0.086f, 0.086f, 1.00f); // #161616
    static const ImVec4 Content = ImVec4(0.090f, 0.090f, 0.090f, 1.00f); // #171717
    static const ImVec4 Header = ImVec4(0.075f, 0.075f, 0.075f, 1.00f); // #131313
    static const ImVec4 Card = ImVec4(0.114f, 0.114f, 0.114f, 1.00f); // #1D1D1D
    static const ImVec4 Frame = ImVec4(0.137f, 0.137f, 0.145f, 1.00f); // control bg
    static const ImVec4 FrameHi = ImVec4(0.175f, 0.175f, 0.185f, 1.00f);
    static const ImVec4 Border = ImVec4(0.165f, 0.165f, 0.165f, 1.00f); // #2A2A2A
    static const ImVec4 Border2 = ImVec4(0.200f, 0.200f, 0.200f, 1.00f); // #333
    static const ImVec4 Accent = ImVec4(0.949f, 0.486f, 0.149f, 1.00f); // #F27C26 orange
    static const ImVec4 AccentHi = ImVec4(0.988f, 0.624f, 0.310f, 1.00f); // #FC9F4F
    static const ImVec4 AccentDim = ImVec4(0.949f, 0.486f, 0.149f, 0.16f);
    static const ImVec4 AccentGl = ImVec4(0.949f, 0.486f, 0.149f, 0.40f);
    static const ImVec4 TextHi = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
    static const ImVec4 TextMid = ImVec4(1.00f, 1.00f, 1.00f, 0.48f);
    static const ImVec4 TextLo = ImVec4(1.00f, 1.00f, 1.00f, 0.26f);
    static const ImVec4 Box = ImVec4(0.098f, 0.098f, 0.098f, 1.00f);
}
static inline ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// ─────────────────────────────────────────────────────────────────────────────
//  NEW STATE  (ported features — keep separate from your existing Globals)
// ─────────────────────────────────────────────────────────────────────────────
namespace NecusState {

    // ---- Weapon picker -------------------------------------------------------
    static const char* kWeapons[] = {
        "Global",
        "Desert Eagle","R8 Revolver","Dual Berettas","Five-SeveN","Glock-18",
        "P2000","USP-S","P250","CZ75-Auto","Tec-9",
        "MAC-10","MP9","MP7","MP5-SD","UMP-45","P90","PP-Bizon",
        "Galil AR","FAMAS","AK-47","M4A4","M4A1-S","SG 553","AUG",
        "SSG 08","AWP","G3SG1","SCAR-20",
        "Nova","XM1014","MAG-7","Sawed-Off","M249","Negev"
    };
    static const int kWeaponCount = IM_ARRAYSIZE(kWeapons);
    static int g_rageWeapon = 0;     // weapon selected while on the Rage tab
    static int g_legitWeapon = 0;     // weapon selected while on the Legit tab

    // ---- Per-weapon Aimbot (Rage) -------------------------------------------
    struct RageAim {
        bool enabled = false;
        bool targets[3] = { true, false, false };   // Players / Lethal first / Smart
        bool autofire = false;
        bool autoscope = false;
        bool quickpeek = false;
        int  fov = 0;
        int  hitchance = 62;
        int  mindmg = 24;
        bool dmgoverride = false;
        bool hitbox[6] = { true, true, false, false, false, false }; // Head Chest Stomach Arms Legs Feet
        int  multipoint = 2;   // Off / Low / Medium / High
        int  minmp = 35;
        // keybinds (Win32 VK; 0 = none)
        int  kb_enabled = 0x70;       // VK_F1
        int  kb_quickpeek = 0x06;       // VK_XBUTTON2 (MOUSE5)
        int  kb_dmgoverride = 'C';
    };
    static RageAim g_aim[kWeaponCount];

    // ---- Global Anti-Aim -----------------------------------------------------
    struct AntiAim {
        bool doubletap = false;  int kb_doubletap = 'E';
        int  dttolerance = 16;
        bool hideshots = false;  int kb_hideshots = 0x05;   // MOUSE4
        bool aa_enabled = false;
        int  pitch = 3;      // Off Default Up Down Minimal
        int  yawbase = 1;      // Local view / At targets
        int  yaw = 1;      // Off 180 Spin Static
        int  jitter = 2;      // Off Offset Center Random
        int  bodyyaw = 2;      // Off Static Jitter Opposite
        bool freestanding = false;  int kb_free = 0x06;        // MOUSE5
        bool fakelag = false;
        int  flmode = 1;      // Static Adaptive Maximum
        int  fllimit = 14;
    };
    static AntiAim g_aa;

    // ---- Skins UI ------------------------------------------------------------
    struct Skins {
        bool knife_on = false;  int knife_model = 1;  int knife_paint = 1;
        bool gloves_on = false;  int glove_model = 1;  int glove_paint = 1;
        int  active = 0;      int paint = 0;        int wear = 5;  int seed = 661;
    };
    static Skins g_skins;

    // ---- Per-weapon Legit aimbot --------------------------------------------
    // Legit keeps its own per-weapon config, separate from Rage. The currently
    // selected weapon's values live in your existing Globals:: (so the backend
    // is untouched); switching weapons swaps the saved slot in/out of Globals.
    struct LegitAim {
        bool  enabled = false;
        int   hitbox = 0;
        float fov = 5.f;
        float smooth = 6.f;
        bool  vis = true;
        bool  ff = false;
    };
    static LegitAim g_legitSlots[kWeaponCount];

    // Mutual exclusion: Rage and Legit aimbots can't both be active.
    static bool RageActive() { for (int i = 0; i < kWeaponCount; ++i) if (g_aim[i].enabled) return true; return false; }
    static bool LegitActive() {
        if (Globals::aimbot_enabled) return true;
        for (int i = 0; i < kWeaponCount; ++i) if (g_legitSlots[i].enabled) return true;
        return false;
    }

    // ---- New Rage config (stub, matches the UI mockup) ----------------------
    struct RageConfig {
        // MAIN
        bool enabled = false;  int kb_enabled = 0x70;
        bool silent_aim = false;  int kb_silent = 0x71;
        bool auto_fire = false;
        bool aim_walls = false;
        int  fov = 180;
        // SELECTION
        int  target = 0;     // Highest Damage/Closest/Lowest HP/Random
        bool hitboxes[6] = { true, true, false, false, false, false };
        bool multipoint[3] = { true, true, false };  int kb_mp = 0;
        int  hitchance = 50;    int kb_hc = 0;
        int  mindmg = 15;    int kb_md = 0;
        bool quickstop = true;  int kb_qs = 0;
        bool quickscope = true;
        // OTHER
        int  history = 2;     // None/Low/Medium/High
        bool delay_shot = true;
        bool rem_recoil = true;
        bool rem_spread = true;
        bool duck_peek = false;
        bool quick_peek = false;  int kb_qp = 0;
        bool doubletap2 = true;
        // ANTI-AIM (stub)
        bool aa_on = true;   int kb_aa = 0;
        bool pitch_open = false;
        int  pitch2 = 1;     // Off/Down/Up/Zero/Minimal
        bool yaw_open = false;
        int  yaw2 = 1;     // Off/180/Spin/Static/Jitter
        bool free_open = false;
        bool free_on = false;  int kb_free2 = 0;
        bool mo_open = false;
        bool mo_on = false;  int kb_mo = 0;
    };
    static RageConfig g_rage;

    // ---- World effects (stub) -----------------------------------------------
    struct WorldEffects {
        bool nightmode2 = false; float nm_col[4] = { 1.f, 1.f, 1.f, 1.f };
        bool fullbright = false; float fb_col[4] = { 1.f, 1.f, 1.f, 1.f };
        bool ovr_sky = false; float sky_col[4] = { 0.5f, 0.7f, 1.f, 1.f };
        bool ovr_sun = false; float sun_col[4] = { 1.f, 0.9f, 0.7f, 1.f };
        bool ovr_fog = false; float fog_col[4] = { 0.7f, 0.8f, 0.9f, 1.f };
        bool ovr_dof = false; int   kb_dof = 0;
        bool wx_open = false;
        int  weather = 0;     // Clear/Rain/Snow/Foggy
        int  removals = 0;     // None/Props/Decals/Both
    };
    static WorldEffects g_worldfx;
}

// ─────────────────────────────────────────────────────────────────────────────
//  BIND SYSTEM
// ─────────────────────────────────────────────────────────────────────────────
enum class BindMode : int { Hold = 0, Toggle = 1, Always = 2 };

struct BindState {
    std::vector<int> vks;                // multiple keys; empty = unbound
    BindMode mode = BindMode::Hold;
    bool* target = nullptr;
    bool     togState = false;
    bool     prevKey = false;
    bool     active = false;
};

static std::unordered_map<std::string, BindState> g_binds;

// Called EVERY frame (even when menu is closed) — applies bind logic to feature bools.
void Menu::UpdateBinds() {
    for (auto& [lbl, b] : g_binds) {
        if (!b.target) continue;
        if (b.vks.empty()) { b.active = false; continue; }
        // any key in list being held counts as "key down"
        bool keyDown = false;
        for (int vk : b.vks)
            if (vk && (GetAsyncKeyState(vk) & 0x8000)) { keyDown = true; break; }
        switch (b.mode) {
        case BindMode::Hold:
            b.active = keyDown;
            *b.target = keyDown;
            break;
        case BindMode::Toggle:
            if (keyDown && !b.prevKey) {
                b.togState = !b.togState;
                *b.target = b.togState;
            }
            b.active = b.togState;
            b.prevKey = keyDown;
            break;
        case BindMode::Always:
            b.active = true;
            *b.target = true;
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  BIND POPUP  (separate floating window, opened by RMB on any feature label)
// ─────────────────────────────────────────────────────────────────────────────
static const char* VkName(int vk) {
    switch (vk) {
    case 0:     return "---";
    case 0x01:  return "MOUSE1"; case 0x02: return "MOUSE2"; case 0x04: return "MOUSE3";
    case 0x05:  return "MOUSE4"; case 0x06: return "MOUSE5";
    case 0x20:  return "SPACE";  case 0x10: return "SHIFT";  case 0x11: return "CTRL";
    case 0x12:  return "ALT";    case 0x09: return "TAB";    case 0x14: return "CAPS";
    case 0x0D:  return "ENTER";  case 0x08: return "BACK";   case 0x23: return "END";
    case 0x24:  return "HOME";   case 0x2D: return "INS";    case 0x2E: return "DEL";
    case 0x70:return "F1"; case 0x71:return "F2"; case 0x72:return "F3"; case 0x73:return "F4";
    case 0x74:return "F5"; case 0x75:return "F6"; case 0x76:return "F7"; case 0x77:return "F8";
    case 0x78:return "F9"; case 0x79:return "F10"; case 0x7A:return "F11"; case 0x7B:return "F12";
    }
    static char buf[8];
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) { buf[0] = (char)vk; buf[1] = 0; return buf; }
    snprintf(buf, sizeof(buf), "K%d", vk);
    return buf;
}

static std::string g_bindPopupLabel;
static ImVec2      g_bindPopupPos;
static bool        g_bindCapturing = false;
static double      g_bindOpenTime = 0.0;
static double      g_bindCaptureStart = 0.0;
static bool        g_bindJustOpened = false; // blocks close-check on the frame popup appears

// per-frame RMB edge detection (set in Menu::Render before tab content)
static bool        g_rmbWasDown = false;
static bool        g_rmbJustClicked = false;

static void OpenBindPopup(const char* label, bool* target, ImVec2 pos) {
    // toggle: if same popup already open, close it
    if (g_bindPopupLabel == label) {
        g_bindPopupLabel.clear();
        g_bindCapturing = false;
        return;
    }
    g_bindPopupLabel = label;
    g_bindPopupPos = pos;
    g_bindCapturing = false;
    g_bindOpenTime = ImGui::GetTime();
    g_bindJustOpened = true;
    auto& b = g_binds[label];
    if (!b.target) b.target = target;
}

// index of the bind slot currently being captured (-1 = none)
static int g_bindCaptureSlot = -1;

static void RenderBindPopup() {
    if (g_bindPopupLabel.empty()) return;

    // Key capture — raw Win32, runs every frame while capturing
    if (g_bindCapturing && g_bindCaptureSlot >= 0 &&
        (ImGui::GetTime() - g_bindCaptureStart) > 0.12)
    {
        BindState& b2 = g_binds[g_bindPopupLabel];
        for (int vk = 1; vk < 256; ++vk) {
            if (vk == VK_INSERT || vk == VK_RBUTTON) continue;
            if (GetAsyncKeyState(vk) & 0x8000) {
                int newVk = (vk == VK_ESCAPE) ? 0 : vk;
                if (g_bindCaptureSlot < (int)b2.vks.size()) {
                    if (newVk == 0)
                        b2.vks.erase(b2.vks.begin() + g_bindCaptureSlot);
                    else
                        b2.vks[g_bindCaptureSlot] = newVk;
                }
                g_bindCapturing = false;
                g_bindCaptureSlot = -1;
                break;
            }
        }
    }

    BindState& b = g_binds[g_bindPopupLabel];

    ImGui::SetNextWindowPos(g_bindPopupPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(240, 0), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.10f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, Col::Border2);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 9.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

    ImGui::Begin(("##bindwnd_" + g_bindPopupLabel).c_str(), nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing);

    float bw = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    // Title
    ImGui::PushStyleColor(ImGuiCol_Text, Col::TextHi);
    ImGui::TextUnformatted(g_bindPopupLabel.c_str());
    ImGui::PopStyleColor();
    float lineY = ImGui::GetCursorScreenPos().y - 1.f;
    dl->AddLine(ImVec2(wp.x + 8, lineY), ImVec2(wp.x + bw + 16, lineY), U32(Col::Border), 1.f);
    ImGui::Dummy(ImVec2(0, 4));

    // --- Bind list (one row per key) ---
    for (int i = 0; i < (int)b.vks.size(); ++i) {
        bool capturing = g_bindCapturing && g_bindCaptureSlot == i;
        const char* keyLabel = capturing ? "Press any key..." : VkName(b.vks[i]);

        ImGui::PushID(i);

        // Key button — fills most of the width
        float removeW = 24.f;
        float keyW = bw - removeW - 4.f;
        ImGui::PushStyleColor(ImGuiCol_Button, capturing ? U32(Col::AccentDim) : U32(Col::Frame));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(Col::AccentGl));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, U32(Col::AccentGl));
        ImGui::PushStyleColor(ImGuiCol_Text, capturing ? U32(Col::Accent) : U32(Col::TextHi));
        if (ImGui::Button(keyLabel, ImVec2(keyW, 28))) {
            if (!capturing) {
                g_bindCapturing = true;
                g_bindCaptureSlot = i;
                g_bindCaptureStart = ImGui::GetTime();
            }
            else {
                g_bindCapturing = false;
                g_bindCaptureSlot = -1;
            }
        }
        ImGui::PopStyleColor(4);

        ImGui::SameLine(0, 4);

        // [x] remove button
        ImGui::PushStyleColor(ImGuiCol_Button, U32(Col::Frame));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(ImVec4(0.8f, 0.2f, 0.2f, 0.35f)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, U32(ImVec4(0.8f, 0.2f, 0.2f, 0.6f)));
        ImGui::PushStyleColor(ImGuiCol_Text, U32(ImVec4(1.f, 0.4f, 0.4f, 0.9f)));
        if (ImGui::Button("x", ImVec2(removeW, 28))) {
            b.vks.erase(b.vks.begin() + i);
            if (g_bindCaptureSlot == i) { g_bindCapturing = false; g_bindCaptureSlot = -1; }
            ImGui::PopStyleColor(4);
            ImGui::PopID();
            break; // vector changed, exit loop
        }
        ImGui::PopStyleColor(4);
        ImGui::PopID();
    }

    // "+ Add Bind" button
    ImGui::PushStyleColor(ImGuiCol_Button, U32(Col::Frame));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(Col::AccentGl));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, U32(Col::AccentGl));
    ImGui::PushStyleColor(ImGuiCol_Text, U32(Col::Accent));
    if (ImGui::Button("+ Add Bind", ImVec2(bw, 28))) {
        b.vks.push_back(0); // 0 = unset, will trigger capture immediately
        int newSlot = (int)b.vks.size() - 1;
        g_bindCapturing = true;
        g_bindCaptureSlot = newSlot;
        g_bindCaptureStart = ImGui::GetTime();
    }
    ImGui::PopStyleColor(4);

    ImGui::Dummy(ImVec2(0, 4));

    // Mode row
    {
        ImVec2 rowPos = ImGui::GetCursorScreenPos();
        const float rowH = 26.f;
        const char* modes[] = { "Hold", "Toggle", "Always" };
        const float spacing = 4.f;
        const float btnW = (bw - spacing * 2.f) / 3.f;

        dl->AddText(ImVec2(rowPos.x, rowPos.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f),
            U32(Col::TextMid), "Mode");

        for (int i = 0; i < 3; ++i) {
            bool sel = ((int)b.mode == i);
            float bx = rowPos.x + i * (btnW + spacing);
            ImGui::SetCursorScreenPos(ImVec2(bx, rowPos.y));
            ImGui::PushID(100 + i);
            ImGui::PushStyleColor(ImGuiCol_Button, sel ? U32(Col::AccentDim) : U32(Col::Frame));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(Col::AccentGl));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, U32(Col::AccentGl));
            ImGui::PushStyleColor(ImGuiCol_Text, sel ? U32(Col::Accent) : U32(Col::TextMid));
            if (ImGui::Button(modes[i], ImVec2(btnW, rowH))) { b.mode = (BindMode)i; b.togState = false; }
            ImGui::PopStyleColor(4);
            ImGui::PopID();
        }
        ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowPos.y + rowH + 4.f));
    }

    float sepY = ImGui::GetCursorScreenPos().y;
    dl->AddLine(ImVec2(wp.x + 8, sepY), ImVec2(wp.x + bw + 16, sepY), U32(Col::Border), 1.f);
    ImGui::Dummy(ImVec2(0, 5));

    ImGui::PushStyleColor(ImGuiCol_Button, U32(Col::Frame));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(Col::FrameHi));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, U32(Col::FrameHi));
    ImGui::PushStyleColor(ImGuiCol_Text, U32(Col::TextMid));
    bool closeClicked = ImGui::Button("Close", ImVec2(bw, 26));
    ImGui::PopStyleColor(4);

    ImVec2 wpos = ImGui::GetWindowPos();
    ImVec2 wsz = ImGui::GetWindowSize();

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    if (g_bindJustOpened) {
        g_bindJustOpened = false;
        return;
    }

    ImVec2 mpos = ImGui::GetIO().MousePos;
    bool hovered = mpos.x >= wpos.x && mpos.x <= wpos.x + wsz.x &&
        mpos.y >= wpos.y && mpos.y <= wpos.y + wsz.y;

    bool gracePeriod = (ImGui::GetTime() - g_bindOpenTime) < 0.20;
    bool escJustPressed = (GetAsyncKeyState(VK_ESCAPE) & 1) != 0;
    bool clickedOutside = !hovered && !gracePeriod &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    if (closeClicked || escJustPressed || clickedOutside) {
        g_bindPopupLabel.clear();
        g_bindCapturing = false;
        g_bindCaptureSlot = -1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SEARCH + LEGACY KEY CAPTURE (for aimbot_key / triggerbot_key pickers)
// ─────────────────────────────────────────────────────────────────────────────
static char  g_SearchBuf[128] = {};
static bool  g_SearchActive = false;
static int* g_kbListening = nullptr;
static bool  g_kbJustClicked = false;
static char  g_cfgName[64] = "default";
static bool  g_showAllBinds = false;

// Global search mode: set to true when rendering all tabs simultaneously for cross-tab search
static bool g_searchMode = false;

// Wrappers that become no-ops when rendering all tabs in search mode
// (avoids BeginChild/EndChild ID conflicts across tab renders)
namespace SC {
    static inline void Begin(const char* id, ImVec2 sz = ImVec2(0, 0)) {
        if (!g_searchMode) ImGui::BeginChild(id, sz, false, ImGuiWindowFlags_NoScrollbar);
    }
    static inline void End() {
        if (!g_searchMode) ImGui::EndChild();
    }
    static inline void SameLine(float gap) {
        if (!g_searchMode) ImGui::SameLine(0, gap);
    }
    static inline float ColW(float gap) {
        if (g_searchMode) return ImGui::GetContentRegionAvail().x;
        return (ImGui::GetContentRegionAvail().x - gap) / 2.f;
    }
}

// Logo texture: set this once from your render backend (see LogoLoader_D3D11.h).
// If left null, a drawn orange fallback emblem is used so the menu still looks right.
static ImTextureID g_logoTex = (ImTextureID)0;
ImFont* g_necusFont = nullptr; // forward-declare so overlay functions can reference it
void NecusSetLogo(ImTextureID tex) { g_logoTex = tex; }

static bool MatchSearch(const char* text) {
    if (!g_SearchActive || g_SearchBuf[0] == '\0') return true;
    std::string label(text), query(g_SearchBuf);
    std::transform(label.begin(), label.end(), label.begin(), ::tolower);
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
    return label.find(query) != std::string::npos;
}

// Legacy capture for explicit key pickers (aimbot_key, triggerbot_key)
static void UpdateKeybindCapture() {
    if (!g_kbListening) return;
    if (g_kbJustClicked) {
        if (GetAsyncKeyState(0x01) & 0x8000) return;
        g_kbJustClicked = false;
    }
    for (int vk = 1; vk < 256; ++vk) {
        if (vk == VK_INSERT) continue;
        if (GetAsyncKeyState(vk) & 0x8000) {
            *g_kbListening = (vk == VK_ESCAPE) ? 0 : vk;
            g_kbListening = nullptr;
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  STYLED WIDGETS
// ─────────────────────────────────────────────────────────────────────────────
namespace UI {

    static const float kHeaderH = 26.f;
    static const float kPadX = 13.f;
    static const float kRound = 8.f;

    struct CardCtx { ImVec2 p0; float w; int matches; const char* title; const char* badge; };
    static CardCtx g_card;

    // Card with centered title + accent underline (+ optional right-side badge).
    // Header is drawn in EndCard so a card with zero search matches can vanish.
    static void BeginCard(const char* title, const char* badge = nullptr) {
        g_card.p0 = ImGui::GetCursorScreenPos();
        g_card.w = ImGui::GetContentRegionAvail().x;
        g_card.matches = 0;
        g_card.title = title;
        g_card.badge = badge;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->ChannelsSplit(2);
        dl->ChannelsSetCurrent(1);                       // content drawn above bg
        ImGui::Dummy(ImVec2(g_card.w, kHeaderH + 3.f));  // reserve header space
        ImGui::Indent(kPadX);
        ImGui::PushID(title);            // scope all widget IDs to this card (no label clashes)
    }

    static void EndCard() {
        ImGui::PopID();
        ImGui::Unindent(kPadX);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // searching + nothing inside matched -> hide the whole card, reclaim its space
        if (g_SearchActive && g_card.matches == 0) {
            dl->ChannelsMerge();
            ImGui::SetCursorScreenPos(g_card.p0);
            return;
        }

        ImGui::Dummy(ImVec2(0, 6.f));
        ImVec2 p1 = ImVec2(g_card.p0.x + g_card.w, ImGui::GetCursorScreenPos().y);

        // header (now that we know the card is visible)
        ImVec2 ts = ImGui::CalcTextSize(g_card.title);
        dl->AddText(ImVec2(g_card.p0.x + (g_card.w - ts.x) * 0.5f,
            g_card.p0.y + (kHeaderH - ts.y) * 0.5f),
            U32(Col::TextHi), g_card.title);
        float ul = 40.f, ux = g_card.p0.x + (g_card.w - ul) * 0.5f, uy = g_card.p0.y + kHeaderH - 2.f;
        dl->AddRectFilled(ImVec2(ux, uy), ImVec2(ux + ul, uy + 2.f), U32(Col::Accent), 1.f);
        if (g_card.badge && g_card.badge[0]) {
            ImVec2 bs = ImGui::CalcTextSize(g_card.badge);
            float by = g_card.p0.y + kHeaderH * 0.5f;
            float bx1 = g_card.p0.x + g_card.w - 9.f, bx0 = bx1 - bs.x - 14.f;
            dl->AddRectFilled(ImVec2(bx0, by - 9.f), ImVec2(bx1, by + 9.f), U32(Col::AccentDim), 5.f);
            dl->AddRect(ImVec2(bx0, by - 9.f), ImVec2(bx1, by + 9.f), U32(Col::AccentGl), 5.f);
            dl->AddText(ImVec2(bx0 + 7.f, by - bs.y * 0.5f), U32(Col::Accent), g_card.badge);
        }

        dl->ChannelsSetCurrent(0);                       // background
        dl->AddRectFilled(g_card.p0, p1, U32(Col::Card), kRound);
        dl->AddLine(ImVec2(g_card.p0.x, g_card.p0.y + kHeaderH),
            ImVec2(p1.x, g_card.p0.y + kHeaderH), U32(Col::Border), 1.f);
        dl->AddRect(g_card.p0, p1, U32(Col::Border), kRound, 0, 1.f);
        dl->ChannelsMerge();
        ImGui::Dummy(ImVec2(0, 8.f));                    // gap between cards
    }

    static void ColTitle(const char* t) {
        ImGui::PushStyleColor(ImGuiCol_Text, Col::TextHi);
        ImGui::TextUnformatted(t);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 3.f));
    }

    // ── Legacy key chip (for aim-key / trigger-key pickers, not the bind system) ──
    static void KeybindChip(const char* id, int* vk, float rightX, float cy) {
        const char* name = (g_kbListening == vk) ? "..." : VkName(*vk);
        ImVec2 ts = ImGui::CalcTextSize(name);
        float  w = ImClamp(ts.x + 14.f, 34.f, 80.f), h = 18.f;
        ImGui::SetCursorScreenPos(ImVec2(rightX - w, cy - h * 0.5f));
        ImGui::PushID(id);
        ImGui::InvisibleButton("kb", ImVec2(w, h));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) { g_kbListening = vk; g_kbJustClicked = true; }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        dl->AddRectFilled(mn, mx, U32(ImVec4(1, 1, 1, hov ? 0.08f : 0.05f)), 4.f);
        ImU32 tc = (g_kbListening == vk) ? U32(Col::Accent) : U32(Col::TextMid);
        dl->AddText(ImVec2(mn.x + (w - ts.x) * 0.5f, mn.y + (h - ts.y) * 0.5f), tc, name);
        ImGui::PopID();
    }

    // ── Checkbox with bind-dot (shown only when a bind is set) ────────────────
    // RMB on label → opens bind popup.
    // kb: optional explicit key picker (aim-key concept, separate from bind system).
    static bool Checkbox(const char* label, bool* v, int* kb = nullptr, bool locked = false) {
        if (!MatchSearch(label)) return false;
        g_card.matches++;
        ImGui::PushID(label);

        float w0 = ImGui::GetContentRegionAvail().x - kPadX;
        ImVec2 p = ImGui::GetCursorScreenPos();
        float rowH = 22.f, box = 16.f, cy = p.y + rowH * 0.5f;
        float boxX = p.x + w0 - box;
        // chip sits left of box when kb is provided
        float chipRight = kb && !locked ? boxX - 8.f : boxX;

        // ── Label hit area (LMB = toggle, RMB = bind popup) ──────────────────
        float labelW = chipRight - p.x - 2.f;
        ImGui::InvisibleButton("row", ImVec2(labelW, rowH));
        bool changed = false;
        if (!locked && ImGui::IsItemClicked(ImGuiMouseButton_Left)) { *v = !*v; changed = true; }
        // Raw RMB detection — bypasses ImGui focus/window-overlap blocking
        if (!locked && g_rmbJustClicked) {
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            if (mousePos.x >= p.x && mousePos.x <= p.x + labelW &&
                mousePos.y >= p.y && mousePos.y <= p.y + rowH) {
                ImVec2 popPos = ImVec2(p.x, p.y + rowH + 2.f);
                OpenBindPopup(label, v, popPos);
            }
        }
        bool hov = !locked && ImGui::IsItemHovered();

        // ── Draw label ────────────────────────────────────────────────────────
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec4 lblCol = locked ? Col::TextLo : (hov ? Col::TextHi : ImVec4(1, 1, 1, 0.78f));
        dl->AddText(ImVec2(p.x, cy - ImGui::GetTextLineHeight() * 0.5f), U32(lblCol), label);

        // ── Bind dot: small circle right after label text (only when bound) ──
        if (!locked) {
            auto it = g_binds.find(label);
            if (it != g_binds.end() && !it->second.vks.empty()) {
                const BindState& bs = it->second;
                ImVec2 ts = ImGui::CalcTextSize(label);
                float  dotCx = p.x + ts.x + 8.f;
                const float dotR = 3.5f;
                bool dotActive = bs.active;
                ImU32 dotFill = dotActive ? U32(Col::Accent)
                    : U32(ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
                dl->AddCircleFilled(ImVec2(dotCx, cy), dotR, dotFill);
            }
        }

        // ── Legacy key chip ───────────────────────────────────────────────────
        if (kb && !locked) KeybindChip(label, kb, chipRight, cy);

        // ── Checkbox square ───────────────────────────────────────────────────
        ImVec2 b0(boxX, cy - box * 0.5f), b1(boxX + box, cy + box * 0.5f);
        ImGui::SetCursorScreenPos(b0);
        ImGui::InvisibleButton("box", ImVec2(box, box));
        if (!locked && ImGui::IsItemClicked()) { *v = !*v; changed = true; }
        ImU32 fill = *v ? U32(locked ? Col::FrameHi : Col::Accent) : U32(Col::Box);
        ImU32 brd = *v ? U32(locked ? Col::Border2 : Col::AccentHi) : U32(Col::Border2);
        dl->AddRectFilled(b0, b1, fill, 4.f);
        dl->AddRect(b0, b1, brd, 4.f, 0, 1.f);
        if (*v) {
            ImU32 ck = locked ? U32(Col::TextLo) : IM_COL32(20, 20, 20, 255);
            dl->AddLine(ImVec2(b0.x + 4.f, cy + 0.5f), ImVec2(b0.x + 6.6f, cy + 3.2f), ck, 1.8f);
            dl->AddLine(ImVec2(b0.x + 6.6f, cy + 3.2f), ImVec2(b1.x - 3.6f, cy - 3.4f), ck, 1.8f);
        }
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + rowH));
        ImGui::PopID();
        return changed;
    }

    // fill slider (float). fmt may include a unit, e.g. "%.0f%%"
    static bool SliderF(const char* label, float* v, float vmin, float vmax, const char* fmt = "%.1f") {
        if (!MatchSearch(label)) return false;
        g_card.matches++;
        ImGui::PushID(label);
        float startX = ImGui::GetCursorPosX();
        float w0 = ImGui::GetContentRegionAvail().x - kPadX;   // leave right padding (symmetric with left)
        // label + right-aligned value on the same line
        ImGui::PushStyleColor(ImGuiCol_Text, Col::TextMid);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        char val[32]; snprintf(val, sizeof(val), fmt, *v);
        ImVec2 vs = ImGui::CalcTextSize(val);
        ImGui::SameLine();
        ImGui::SetCursorPosX(startX + w0 - vs.x);
        ImGui::PushStyleColor(ImGuiCol_Text, Col::TextHi);
        ImGui::TextUnformatted(val);
        ImGui::PopStyleColor();
        // track
        ImVec2 p = ImGui::GetCursorScreenPos();
        float th = 14.f;
        ImGui::InvisibleButton("trk", ImVec2(w0, th));
        bool active = ImGui::IsItemActive();
        if (active) {
            float t = ImClamp((ImGui::GetIO().MousePos.x - p.x) / w0, 0.f, 1.f);
            *v = vmin + t * (vmax - vmin);
        }
        float cy = p.y + th * 0.5f, t = (*v - vmin) / (vmax - vmin);
        float fx = p.x + t * w0;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(ImVec2(p.x, cy - 2.f), ImVec2(p.x + w0, cy + 2.f), U32(Col::Frame), 2.f);
        dl->AddRectFilled(ImVec2(p.x, cy - 2.f), ImVec2(fx, cy + 2.f), U32(Col::Accent), 2.f);
        dl->AddCircleFilled(ImVec2(fx, cy), 5.f, U32(Col::AccentHi));
        ImGui::PopID();
        return active;
    }

    static bool SliderI(const char* label, int* v, int vmin, int vmax, const char* fmt = "%.0f") {
        float f = (float)*v;
        bool a = SliderF(label, &f, (float)vmin, (float)vmax, fmt);
        if (a) *v = (int)(f + 0.5f);
        return a;
    }

    // a single option row inside a combo popup (orange check for multi / orange dot for single)
    static bool OptionRow(const char* label, bool selected, bool multi, float width) {
        ImGui::PushID(label);
        float h = 27.f;
        ImGui::InvisibleButton("o", ImVec2(width, h));
        bool clicked = ImGui::IsItemClicked();
        bool hov = ImGui::IsItemHovered();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        float cy = (mn.y + mx.y) * 0.5f;
        if (hov) dl->AddRectFilled(mn, mx, U32(ImVec4(1, 1, 1, 0.04f)), 5.f);
        float mkx = mn.x + 8.f;
        if (multi) {
            ImVec2 b0(mkx, cy - 6.5f), b1(mkx + 13.f, cy + 6.5f);
            if (selected) {
                dl->AddRectFilled(b0, b1, U32(Col::Accent), 3.f);
                dl->AddLine(ImVec2(b0.x + 3.f, cy + 0.3f), ImVec2(b0.x + 5.6f, cy + 2.8f), IM_COL32(26, 26, 26, 255), 1.7f);
                dl->AddLine(ImVec2(b0.x + 5.6f, cy + 2.8f), ImVec2(b1.x - 2.6f, cy - 3.2f), IM_COL32(26, 26, 26, 255), 1.7f);
            }
            else {
                dl->AddRect(b0, b1, U32(Col::Border2), 3.f, 0, 1.f);
            }
        }
        else if (selected) {
            dl->AddCircleFilled(ImVec2(mkx + 6.5f, cy), 3.2f, U32(Col::Accent));
        }
        ImU32 tc = selected ? U32(Col::Accent) : U32(hov ? Col::TextHi : Col::TextMid);
        dl->AddText(ImVec2(mkx + 22.f, cy - ImGui::GetTextLineHeight() * 0.5f), tc, label);
        ImGui::PopID();
        return clicked;
    }

    // draws the "label .......... [ value  v ]" row; returns true when the box is clicked.
    // box screen-rect + width returned via out-params so the caller can anchor the popup.
    static bool comboCore(const char* label, const char* disp, bool empty,
        ImVec2& mnOut, ImVec2& mxOut, float& boxWOut) {
        float w0 = ImGui::GetContentRegionAvail().x - kPadX;
        ImVec2 p = ImGui::GetCursorScreenPos();
        float rowH = 26.f, cy = p.y + rowH * 0.5f;
        float boxW = ImClamp(w0 * 0.5f, 96.f, 150.f);
        float boxX = p.x + w0 - boxW;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddText(ImVec2(p.x, cy - ImGui::GetTextLineHeight() * 0.5f), U32(Col::TextMid), label);
        ImGui::SetCursorScreenPos(ImVec2(boxX, cy - 13.f));
        ImGui::InvisibleButton("cb", ImVec2(boxW, 26.f));
        bool clicked = ImGui::IsItemClicked();
        bool open = ImGui::IsPopupOpen("##pop");
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        dl->AddRectFilled(mn, mx, U32(Col::Box), 6.f);
        dl->AddRect(mn, mx, U32(open ? Col::Accent : Col::Border), 6.f, 0, 1.f);
        dl->PushClipRect(ImVec2(mn.x + 9, mn.y), ImVec2(mx.x - 20, mx.y), true);
        dl->AddText(ImVec2(mn.x + 9, cy - ImGui::GetTextLineHeight() * 0.5f),
            U32(empty ? Col::TextMid : Col::TextHi), disp);
        dl->PopClipRect();
        float ax = mx.x - 13, ay = cy + 1;
        dl->AddLine(ImVec2(ax - 4, ay - 2), ImVec2(ax, ay + 2), U32(Col::TextMid), 1.4f);
        dl->AddLine(ImVec2(ax, ay + 2), ImVec2(ax + 4, ay - 2), U32(Col::TextMid), 1.4f);
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + rowH));
        mnOut = mn; mxOut = mx; boxWOut = boxW;
        return clicked;
    }

    static void stylePopup(const ImVec2& mn, const ImVec2& mx, float boxW) {
        if (ImGui::IsPopupOpen("##pop")) {
            ImGui::SetNextWindowPos(ImVec2(mn.x, mx.y + 4));
            ImGui::SetNextWindowSizeConstraints(ImVec2(boxW, 0), ImVec2(boxW + 90.f, 360.f));
        }
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.118f, 0.118f, 0.118f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Border, Col::Border2);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 7.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
    }

    // styled single-select combo (label left, box right — gamesense popup)
    static bool Combo(const char* label, int* cur, const char* const items[], int count) {
        if (!MatchSearch(label)) return false;
        g_card.matches++;
        ImGui::PushID(label);
        ImVec2 mn, mx; float boxW;
        if (comboCore(label, items[*cur], false, mn, mx, boxW)) ImGui::OpenPopup("##pop");
        bool changed = false;
        stylePopup(mn, mx, boxW);
        if (ImGui::BeginPopup("##pop")) {
            float pw = ImGui::GetContentRegionAvail().x;
            for (int i = 0; i < count; ++i)
                if (OptionRow(items[i], i == *cur, false, pw)) { *cur = i; changed = true; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::PopID();
        return changed;
    }

    // styled multi-select combo (orange checkmarks in the popup)
    static void MultiCombo(const char* label, const char* const items[], bool* flags, int count) {
        if (!MatchSearch(label)) return;
        g_card.matches++;
        ImGui::PushID(label);
        std::string summary; int n = 0;
        for (int i = 0; i < count; ++i) if (flags[i]) { if (n) summary += ", "; summary += items[i]; ++n; }
        char buf[48]; const char* disp;
        if (n == 0) disp = "None";
        else if (n > 2) { snprintf(buf, sizeof(buf), "%d selected", n); disp = buf; }
        else disp = summary.c_str();
        ImVec2 mn, mx; float boxW;
        if (comboCore(label, disp, n == 0, mn, mx, boxW)) ImGui::OpenPopup("##pop");
        stylePopup(mn, mx, boxW);
        if (ImGui::BeginPopup("##pop")) {
            float pw = ImGui::GetContentRegionAvail().x;
            for (int i = 0; i < count; ++i)
                if (OptionRow(items[i], flags[i], true, pw)) flags[i] = !flags[i];
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }

    static void Color(const char* label, float* rgba) {
        if (!MatchSearch(label)) return;
        g_card.matches++;
        ImGui::PushID(label);
        ImGui::PushStyleColor(ImGuiCol_Text, Col::TextMid);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - 22.f - kPadX);
        ImGui::ColorEdit4("##col", rgba, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        ImGui::PopID();
    }

    static bool StyledButton(const char* label, const ImVec2& size, bool primary = false) {
        ImGui::PushStyleColor(ImGuiCol_Button, primary ? Col::AccentDim : Col::Frame);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, primary ? Col::AccentGl : Col::FrameHi);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, primary ? Col::AccentGl : Col::FrameHi);
        ImGui::PushStyleColor(ImGuiCol_Text, primary ? Col::Accent : Col::TextHi);
        bool r = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return r;
    }

    // Expandable row: label left, animated ">" right. Returns true when open.
    // Pass a unique id and a bool* for expand state.
    static bool ExpandRow(const char* label, bool* open, int* kb = nullptr) {
        if (!MatchSearch(label)) return *open;
        g_card.matches++;
        ImGui::PushID(label);
        float w0 = ImGui::GetContentRegionAvail().x - kPadX;
        ImVec2 p = ImGui::GetCursorScreenPos();
        float rowH = 22.f, cy = p.y + rowH * 0.5f;
        ImGui::InvisibleButton("exprow", ImVec2(w0, rowH));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) *open = !*open;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 tc = hov ? U32(Col::TextHi) : U32(ImVec4(1, 1, 1, 0.78f));
        dl->AddText(ImVec2(p.x, cy - ImGui::GetTextLineHeight() * 0.5f), tc, label);
        float ax = p.x + w0 - 8.f;
        if (kb) KeybindChip(label, kb, ax - 10.f, cy);
        if (*open) {
            dl->AddLine(ImVec2(ax - 4, cy - 2), ImVec2(ax, cy + 2), U32(Col::Accent), 1.5f);
            dl->AddLine(ImVec2(ax, cy + 2), ImVec2(ax + 4, cy - 2), U32(Col::Accent), 1.5f);
        }
        else {
            dl->AddLine(ImVec2(ax - 2, cy - 4), ImVec2(ax + 2, cy), U32(Col::TextMid), 1.5f);
            dl->AddLine(ImVec2(ax + 2, cy), ImVec2(ax - 2, cy + 4), U32(Col::TextMid), 1.5f);
        }
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + rowH));
        ImGui::PopID();
        return *open;
    }

    // Color swatch inline with label, toggled by a checkbox on the right
    static void ColorToggle(const char* label, float* rgba, bool* enabled) {
        if (!MatchSearch(label)) return;
        g_card.matches++;
        ImGui::PushID(label);
        float w0 = ImGui::GetContentRegionAvail().x - kPadX;
        ImVec2 p = ImGui::GetCursorScreenPos();
        float rowH = 22.f, cy = p.y + rowH * 0.5f;
        float box = 16.f, sw = 20.f;
        // color swatch
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 s0(p.x, cy - sw * 0.5f + 2), s1(p.x + sw, cy + sw * 0.5f - 2);
        dl->AddRectFilled(s0, s1, ImGui::ColorConvertFloat4ToU32(ImVec4(rgba[0], rgba[1], rgba[2], 1)), 3.f);
        dl->AddRect(s0, s1, U32(Col::Border2), 3.f);
        ImGui::SetCursorScreenPos(ImVec2(p.x, cy - sw * 0.5f + 2));
        ImGui::InvisibleButton("sw", ImVec2(sw, sw - 4));
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("##colpick");
        if (ImGui::BeginPopup("##colpick")) {
            ImGui::ColorPicker4("##cp", rgba, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
            ImGui::EndPopup();
        }
        // label
        dl->AddText(ImVec2(p.x + sw + 6, cy - ImGui::GetTextLineHeight() * 0.5f), U32(Col::TextMid), label);
        // toggle box
        float boxX = p.x + w0 - box;
        ImVec2 b0(boxX, cy - box * 0.5f), b1(boxX + box, cy + box * 0.5f);
        ImGui::SetCursorScreenPos(b0);
        ImGui::InvisibleButton("box", ImVec2(box, box));
        if (ImGui::IsItemClicked()) *enabled = !*enabled;
        ImU32 fill = *enabled ? U32(Col::Accent) : U32(Col::Box);
        ImU32 brd = *enabled ? U32(Col::AccentHi) : U32(Col::Border2);
        dl->AddRectFilled(b0, b1, fill, 4.f);
        dl->AddRect(b0, b1, brd, 4.f, 0, 1.f);
        if (*enabled) {
            dl->AddLine(ImVec2(b0.x + 4, cy + 0.5f), ImVec2(b0.x + 6.6f, cy + 3.2f), IM_COL32(20, 20, 20, 255), 1.8f);
            dl->AddLine(ImVec2(b0.x + 6.6f, cy + 3.2f), ImVec2(b1.x - 3.6f, cy - 3.4f), IM_COL32(20, 20, 20, 255), 1.8f);
        }
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + rowH));
        ImGui::PopID();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SIDEBAR ICONS  (drawn with the draw list)
// ─────────────────────────────────────────────────────────────────────────────
static void DrawTabIcon(ImDrawList* dl, ImVec2 c, int tab, ImU32 col) {
    const float s = 11.f, th = 1.7f;
    switch (tab) {
    case 0: // Rage — crosshair (circle + outside ticks + center dot)
        dl->AddCircle(c, s, col, 28, th);
        dl->AddLine(ImVec2(c.x, c.y - s - 4), ImVec2(c.x, c.y - s), col, th);
        dl->AddLine(ImVec2(c.x, c.y + s), ImVec2(c.x, c.y + s + 4), col, th);
        dl->AddLine(ImVec2(c.x - s - 4, c.y), ImVec2(c.x - s, c.y), col, th);
        dl->AddLine(ImVec2(c.x + s, c.y), ImVec2(c.x + s + 4, c.y), col, th);
        dl->AddCircleFilled(c, 2.0f, col);
        break;
    case 1: { // Legit — house (roof + walls + door)
        float r = s + 1;
        dl->AddLine(ImVec2(c.x - r, c.y - 1), ImVec2(c.x, c.y - s - 2), col, th);
        dl->AddLine(ImVec2(c.x, c.y - s - 2), ImVec2(c.x + r, c.y - 1), col, th);
        dl->AddLine(ImVec2(c.x - s + 1, c.y - 1), ImVec2(c.x - s + 1, c.y + s), col, th);
        dl->AddLine(ImVec2(c.x + s - 1, c.y - 1), ImVec2(c.x + s - 1, c.y + s), col, th);
        dl->AddLine(ImVec2(c.x - s + 1, c.y + s), ImVec2(c.x + s - 1, c.y + s), col, th);
        dl->AddLine(ImVec2(c.x - 2.5f, c.y + s), ImVec2(c.x - 2.5f, c.y + 3), col, th);
        dl->AddLine(ImVec2(c.x + 2.5f, c.y + s), ImVec2(c.x + 2.5f, c.y + 3), col, th);
        dl->AddLine(ImVec2(c.x - 2.5f, c.y + 3), ImVec2(c.x + 2.5f, c.y + 3), col, th);
        break;
    }
    case 2: { // Visuals — eye (almond outline + pupil)
        ImVec2 L(c.x - s, c.y), R(c.x + s, c.y);
        dl->AddBezierQuadratic(L, ImVec2(c.x, c.y - s * 0.85f), R, col, th, 0);
        dl->AddBezierQuadratic(L, ImVec2(c.x, c.y + s * 0.85f), R, col, th, 0);
        dl->AddCircleFilled(c, 3.0f, col);
        break;
    }
    case 3: { // Skins — pencil (diagonal body + tip + cap)
        ImVec2 a(c.x - s + 1, c.y + s - 1);   // tip (bottom-left)
        ImVec2 b(c.x + s - 2, c.y - s + 2);   // cap (top-right)
        ImVec2 dir(b.x - a.x, b.y - a.y);
        float len = ImSqrt(dir.x * dir.x + dir.y * dir.y); dir.x /= len; dir.y /= len;
        ImVec2 nrm(-dir.y, dir.x);
        float hw = 3.0f;
        ImVec2 t0(a.x + dir.x * 5, a.y + dir.y * 5);   // base of the sharpened tip
        dl->AddQuadFilled(ImVec2(t0.x + nrm.x * hw, t0.y + nrm.y * hw),
            ImVec2(t0.x - nrm.x * hw, t0.y - nrm.y * hw),
            ImVec2(b.x - nrm.x * hw, b.y - nrm.y * hw),
            ImVec2(b.x + nrm.x * hw, b.y + nrm.y * hw), col);
        dl->AddTriangleFilled(a, ImVec2(t0.x + nrm.x * hw, t0.y + nrm.y * hw),
            ImVec2(t0.x - nrm.x * hw, t0.y - nrm.y * hw), col);
        dl->AddLine(ImVec2(b.x + nrm.x * hw, b.y + nrm.y * hw),
            ImVec2(b.x - nrm.x * hw, b.y - nrm.y * hw), col, th);
        break;
    }
    case 4: { // Misc — people
        // main person (left)
        dl->AddCircle(ImVec2(c.x - 3, c.y - 4), 3.4f, col, 16, th);
        dl->PathArcTo(ImVec2(c.x - 3, c.y + 7), 6.5f, IM_PI * 1.12f, IM_PI * 1.88f, 20);
        dl->PathStroke(col, 0, th);
        // smaller person (right, behind)
        dl->AddCircle(ImVec2(c.x + 5, c.y - 1), 2.6f, col, 14, th);
        dl->PathArcTo(ImVec2(c.x + 5, c.y + 8), 5.0f, IM_PI * 1.15f, IM_PI * 1.85f, 18);
        dl->PathStroke(col, 0, th);
        break;
    }
    case 5: // Config — sliders
        for (int i = -1; i <= 1; ++i) { float y = c.y + i * 6.f; dl->AddLine(ImVec2(c.x - s, y), ImVec2(c.x + s, y), col, th); dl->AddCircleFilled(ImVec2(c.x + i * 5.f, y), 3.f, col); }
        break;
    case 6: // Scripts — code </>
        dl->AddLine(ImVec2(c.x - 3, c.y - s + 3), ImVec2(c.x - s, c.y), col, th);
        dl->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x - 3, c.y + s - 3), col, th);
        dl->AddLine(ImVec2(c.x + 3, c.y - s + 3), ImVec2(c.x + s, c.y), col, th);
        dl->AddLine(ImVec2(c.x + s, c.y), ImVec2(c.x + 3, c.y + s - 3), col, th);
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  HEADER ICONS  (Save glyph + weapon/loadout glyph)
// ─────────────────────────────────────────────────────────────────────────────
static void DrawSaveIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    const float th = 1.6f;
    // down arrow
    dl->AddLine(ImVec2(c.x, c.y - 5), ImVec2(c.x, c.y + 2.5f), col, th);
    dl->AddLine(ImVec2(c.x - 3, c.y - 0.5f), ImVec2(c.x, c.y + 2.5f), col, th);
    dl->AddLine(ImVec2(c.x + 3, c.y - 0.5f), ImVec2(c.x, c.y + 2.5f), col, th);
    // tray (open-top U)
    dl->AddLine(ImVec2(c.x - 6, c.y + 3), ImVec2(c.x - 6, c.y + 6), col, th);
    dl->AddLine(ImVec2(c.x - 6, c.y + 6), ImVec2(c.x + 6, c.y + 6), col, th);
    dl->AddLine(ImVec2(c.x + 6, c.y + 6), ImVec2(c.x + 6, c.y + 3), col, th);
}
static void DrawWeaponIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    // slide / barrel
    dl->AddRectFilled(ImVec2(c.x - 8, c.y - 4), ImVec2(c.x + 7, c.y - 1), col, 1.f);
    dl->AddRectFilled(ImVec2(c.x + 6, c.y - 3.5f), ImVec2(c.x + 8.5f, c.y - 1), col, 0.f);
    // grip (angled)
    dl->AddQuadFilled(ImVec2(c.x - 4, c.y - 1), ImVec2(c.x - 1, c.y - 1),
        ImVec2(c.x - 2.5f, c.y + 5), ImVec2(c.x - 5.5f, c.y + 5), col);
    // trigger
    dl->AddLine(ImVec2(c.x, c.y - 1), ImVec2(c.x + 0.5f, c.y + 2), col, 1.4f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  TABS
// ─────────────────────────────────────────────────────────────────────────────
static void RenderRageTab() {
    using namespace NecusState;
    RageConfig& R = g_rage;

    float gap = 14.f, colW = (ImGui::GetContentRegionAvail().x - gap) / 2.f;

    // ── Column A: MAIN + SELECTION ───────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("rageA", ImVec2(colW, 0), false, ImGuiWindowFlags_NoScrollbar);
    UI::ColTitle("Main");

    UI::BeginCard("Main");
    {
        UI::Checkbox("Enabled", &R.enabled, &R.kb_enabled);
        UI::Checkbox("Silent Aim", &R.silent_aim, &R.kb_silent);
        UI::Checkbox("Automatic Fire", &R.auto_fire);
        UI::Checkbox("Aim Through Walls", &R.aim_walls);
        UI::SliderI("Field of View", &R.fov, 0, 180, "%.1f\xC2\xB0");
    }
    UI::EndCard();

    UI::ColTitle("Selection");

    UI::BeginCard("Selection");
    {
        static const char* tgt[] = { "Highest Damage", "Closest", "Lowest HP", "Random" };
        UI::Combo("Target", &R.target, tgt, 4);
        static const char* hb[] = { "Head", "Chest", "Stomach", "Arms", "Legs", "Feet" };
        UI::MultiCombo("Hitboxes", hb, R.hitboxes, 6);
        static const char* mp[] = { "Head", "Chest", "Stomach" };
        UI::MultiCombo("Multipoint", mp, R.multipoint, 3);
        UI::SliderI("Hit Chance", &R.hitchance, 0, 100, "%.0f%%");
        UI::SliderI("Min Damage", &R.mindmg, 0, 250, "%.0f");
        UI::Checkbox("Quick Stop", &R.quickstop, &R.kb_qs);
        UI::Checkbox("Quick Scope", &R.quickscope);
    }
    UI::EndCard();
    ImGui::EndChild();

    ImGui::SameLine(0, gap);

    // ── Column B: OTHER + ANTI-AIM ───────────────────────────────────────────
    ImGui::BeginChild("rageB", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
    UI::ColTitle("Other");

    UI::BeginCard("Other");
    {
        static const char* hist[] = { "None", "Low", "Medium", "High" };
        UI::Combo("History", &R.history, hist, 4);
        UI::Checkbox("Delay Shot", &R.delay_shot);
        UI::Checkbox("Remove Recoil", &R.rem_recoil);
        UI::Checkbox("Remove Spread", &R.rem_spread);
        UI::Checkbox("Duck Peek Assist", &R.duck_peek);
        UI::Checkbox("Quick Peek Assist", &R.quick_peek, &R.kb_qp);
        UI::Checkbox("Double Tap", &R.doubletap2);
    }
    UI::EndCard();

    UI::ColTitle("Anti-Aim");

    UI::BeginCard("Anti-Aim");
    {
        UI::Checkbox("Enabled", &R.aa_on, &R.kb_aa);
        if (R.aa_on) {
            // Pitch sub-row
            if (UI::ExpandRow("Pitch", &R.pitch_open)) {
                static const char* p[] = { "Off", "Down", "Up", "Zero", "Minimal" };
                ImGui::Indent(14.f);
                UI::Combo("##pitch2", &R.pitch2, p, 5);
                ImGui::Unindent(14.f);
            }
            // Yaw sub-row
            if (UI::ExpandRow("Yaw", &R.yaw_open)) {
                static const char* y[] = { "Off", "180", "Spin", "Static", "Jitter" };
                ImGui::Indent(14.f);
                UI::Combo("##yaw2", &R.yaw2, y, 5);
                ImGui::Unindent(14.f);
            }
            // Freestanding sub-row
            if (UI::ExpandRow("Freestanding", &R.free_open)) {
                ImGui::Indent(14.f);
                UI::Checkbox("Enable##free", &R.free_on, &R.kb_free2);
                ImGui::Unindent(14.f);
            }
            // Mouse Override sub-row
            if (UI::ExpandRow("Mouse Override", &R.mo_open)) {
                ImGui::Indent(14.f);
                UI::Checkbox("Enable##mo", &R.mo_on, &R.kb_mo);
                ImGui::Unindent(14.f);
            }
        }
    }
    UI::EndCard();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderLegitTab() {
    using namespace NecusState;
    const char* badge = kWeapons[g_legitWeapon];

    float gap = 14.f, colW = (ImGui::GetContentRegionAvail().x - gap) / 2.f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

    // ── Left column: GLOBALS + EXTRA ─────────────────────────────────────────
    ImGui::BeginChild("legitA", ImVec2(colW, 0), false, ImGuiWindowFlags_NoScrollbar);
    UI::ColTitle("Globals");
    UI::BeginCard("Globals");
    {
        static const char* dw[] = { "None", "Smoke", "Flash", "Reloading" };
        UI::Combo("Disable when", &Globals::aimbot_disable_when, dw, 4);
        UI::Checkbox("Auto wall", &Globals::aimbot_auto_wall);
        UI::Checkbox("Auto fire", &Globals::aimbot_auto_fire);
        UI::Checkbox("Auto scope", &Globals::aimbot_auto_scope);
        UI::Checkbox("VAC-live mode", &Globals::aimbot_vac_live);
        UI::ColorToggle("Draw FOV", Globals::aimbot_fov_color, &Globals::aimbot_draw_fov);
        UI::Checkbox("Silent aim", &Globals::aimbot_silent);
        UI::Checkbox("Friendly fire", &Globals::aimbot_friendly_fire);
    }
    UI::EndCard();

    UI::ColTitle("Extra");
    UI::BeginCard("Extra");
    {
        UI::SliderF("Min damage", &Globals::aimbot_min_damage, 0.f, 100.f, "%.0f hp");
        UI::SliderF("Hit chance", &Globals::aimbot_hit_chance, 0.f, 100.f, "%.0f%%");
        UI::Checkbox("Enable RCS", &Globals::rcs_enabled);
        if (Globals::rcs_enabled) {
            UI::SliderF("Pitch scale", &Globals::rcs_x_scale, 0.f, 2.f, "%.2f");
            UI::SliderF("Yaw scale", &Globals::rcs_y_scale, 0.f, 2.f, "%.2f");
        }
    }
    UI::EndCard();
    ImGui::EndChild();

    ImGui::SameLine(0, gap);

    // ── Right column: TARGET + Triggerbot ────────────────────────────────────
    ImGui::BeginChild("legitB", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
    UI::ColTitle("Target");
    UI::BeginCard("Target", badge);
    {
        UI::Checkbox("Enabled", &Globals::aimbot_enabled);
        if (Globals::aimbot_enabled) {
            static const char* hb[] = { "Head", "Neck", "Chest", "Stomach", "Arms", "Legs" };
            UI::MultiCombo("Hitboxes", hb, Globals::aimbot_hitboxes, 6);
            UI::SliderF("FOV", &Globals::aimbot_fov, 0.f, 45.f, "%.2f");
            UI::SliderF("Smooth", &Globals::aimbot_smooth, 1.f, 20.f, "%.2f");
            UI::SliderF("Shot delay", &Globals::aimbot_shot_delay, 0.f, 0.5f, "%.2f");
            UI::SliderF("Kill delay", &Globals::aimbot_kill_delay, 0.f, 1.0f, "%.2f");
            UI::Checkbox("Lock target", &Globals::aimbot_lock_target);
            UI::Checkbox("Lock mouse", &Globals::aimbot_lock_mouse);
        }
    }
    UI::EndCard();

    UI::ColTitle("Triggerbot");
    UI::BeginCard("Triggerbot");
    {
        UI::Checkbox("Enable Triggerbot", &Globals::triggerbot_enabled);
        if (Globals::triggerbot_enabled) {
            UI::SliderI("Delay (ms)", &Globals::triggerbot_delay, 0, 500, "%.0f");
            static const char* tb[] = { "Any hitbox", "Head only" };
            UI::Combo("Hitbox filter", &Globals::triggerbot_hitbox_filter, tb, IM_ARRAYSIZE(tb));
        }
        UI::Checkbox("Enable Backtrack", &Globals::backtrack_enabled);
        if (Globals::backtrack_enabled)
            UI::SliderF("Time (ms)", &Globals::backtrack_time, 0.f, 200.f, "%.0f");
    }
    UI::EndCard();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static int g_visualsSub = 0; // 0 enemies / 1 teammates / 2 world
static void SegmentedSubTabs(const char* const labels[], int count, int* cur) {
    float w = (ImGui::GetContentRegionAvail().x) / count;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));
    for (int i = 0; i < count; ++i) {
        bool sel = (*cur == i);
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? Col::AccentDim : Col::Frame);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sel ? Col::AccentGl : Col::FrameHi);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::AccentGl);
        ImGui::PushStyleColor(ImGuiCol_Text, sel ? Col::Accent : Col::TextMid);
        if (ImGui::Button(labels[i], ImVec2(w - 6, 28))) *cur = i;
        ImGui::PopStyleColor(4);
        if (i < count - 1) ImGui::SameLine();
    }
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0, 10));
}

static void RenderVisualsTab() {
    const char* subs[] = { "Enemies", "Teammates", "World" };
    SegmentedSubTabs(subs, 3, &g_visualsSub);

    float gap = 14.f, colW = (ImGui::GetContentRegionAvail().x - gap) / 2.f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

    if (g_visualsSub == 0 || g_visualsSub == 1) {
        bool e = (g_visualsSub == 0);
        float gap2 = 12.f;
        float avail = ImGui::GetContentRegionAvail().x;
        float leftW = avail * 0.54f;
        float rightW = avail - leftW - gap2;

        // ── Left: feature list ─────────────────────────────────────────────────────
        ImGui::BeginChild("##vis_L", ImVec2(leftW, 0), false, ImGuiWindowFlags_NoScrollbar);

        UI::BeginCard(e ? "Enemy Options" : "Teammate Options");
        UI::Checkbox("Enable", e ? &Globals::esp_enabled : &Globals::esp_teammates);
        UI::Checkbox("Skeleton", e ? &Globals::enemy_skeleton : &Globals::team_skeleton);
        if (*(e ? &Globals::enemy_skeleton : &Globals::team_skeleton)) {
            UI::Color("Bone Color", e ? Globals::esp_skeleton_color : Globals::team_skeleton_color);
            UI::SliderF("Bone Thickness", &Globals::esp_skeleton_thickness, 1.f, 4.f, "%.1f");
        }
        UI::Checkbox("Glow", e ? &Globals::enemy_glow_enabled : &Globals::team_glow_enabled);
        if (*(e ? &Globals::enemy_glow_enabled : &Globals::team_glow_enabled)) {
            UI::Color("Glow Color", e ? Globals::enemy_glow_color : Globals::team_glow_color);
            UI::SliderF("Brightness", e ? &Globals::enemy_glow_brightness : &Globals::team_glow_brightness, 0.1f, 3.0f, "%.2f");
            static const char* gSt[] = { "Normal", "Saturated", "Outline", "Thick" };
            UI::Combo("Style", e ? &Globals::enemy_glow_style : &Globals::team_glow_style, gSt, 4);
            UI::Checkbox("Skeleton Glow", e ? &Globals::enemy_glow_skeleton : &Globals::team_glow_skeleton);
        }
        UI::Checkbox("Chams", e ? &Globals::enemy_chams_enabled : &Globals::team_chams_enabled);
        if (*(e ? &Globals::enemy_chams_enabled : &Globals::team_chams_enabled)) {
            static const char* chamsSt[] = { "Flat", "Illuminate", "Glow" };
            UI::Combo("Material", e ? &Globals::enemy_chams_material : &Globals::team_chams_material, chamsSt, 3);
            UI::Color("Visible Color", e ? Globals::enemy_chams_visible_color : Globals::team_chams_visible_color);
            UI::Checkbox("XQZ (Through Wall)", e ? &Globals::enemy_chams_xqz_enabled : &Globals::team_chams_xqz_enabled);
            if (*(e ? &Globals::enemy_chams_xqz_enabled : &Globals::team_chams_xqz_enabled))
                UI::Color("XQZ Color", e ? Globals::enemy_chams_invisible_color : Globals::team_chams_invisible_color);
        }
        UI::EndCard();

        // Manage Elements
        {
            static bool s_elemE = false, s_elemT = false;
            bool& elOpen = e ? s_elemE : s_elemT;
            UI::BeginCard("Elements");

            // Manage button
            {
                ImDrawList* dL = ImGui::GetWindowDrawList();
                float bw2 = ImGui::GetContentRegionAvail().x - 13.f;
                ImVec2 bp2 = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##elem_btn", ImVec2(bw2, 26.f));
                bool bhov = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked()) elOpen = !elOpen;
                dL->AddRectFilled(bp2, ImVec2(bp2.x + bw2, bp2.y + 26.f),
                    elOpen ? U32(Col::AccentDim) : U32(ImVec4(0.14f, 0.14f, 0.14f, 1.f)), 5.f);
                if (elOpen || bhov)
                    dL->AddRect(bp2, ImVec2(bp2.x + bw2, bp2.y + 26.f), U32(Col::AccentGl), 5.f);
                const char* lbl2 = elOpen ? "▲  Elements" : "▼  Elements";
                ImVec2 lsz = ImGui::CalcTextSize(lbl2);
                dL->AddText(ImVec2(bp2.x + (bw2 - lsz.x) * 0.5f, bp2.y + (26.f - lsz.y) * 0.5f),
                    elOpen ? U32(Col::Accent) : U32(Col::TextMid), lbl2);
            }

            if (elOpen) {
                ImGui::Dummy(ImVec2(0, 4.f));
                UI::Checkbox("Box", e ? &Globals::enemy_box : &Globals::team_box);
                if (*(e ? &Globals::enemy_box : &Globals::team_box)) {
                    static const char* bst[] = { "2D", "Corners" };
                    UI::Combo("Style", e ? &Globals::enemy_box_style : &Globals::team_box_style, bst, 2);
                    UI::Checkbox("Fill", e ? &Globals::enemy_box_fill : &Globals::team_box_fill);
                    if (*(e ? &Globals::enemy_box_fill : &Globals::team_box_fill))
                        UI::Color("Fill Color", e ? Globals::enemy_box_fill_color : Globals::team_box_fill_color);
                    UI::Color("Box Color", e ? Globals::esp_box_color : Globals::esp_teammate_color);
                    UI::SliderF("Thickness", &Globals::esp_box_thickness, 1.f, 4.f, "%.1f");
                }
                UI::Checkbox("Health Bar", e ? &Globals::enemy_health : &Globals::team_health);
                if (*(e ? &Globals::enemy_health : &Globals::team_health)) {
                    static const char* hpos[] = { "Top", "Bottom", "Left", "Right" };
                    UI::Combo("HP Pos", e ? &Globals::enemy_health_position : &Globals::team_health_position, hpos, 4);
                    UI::Checkbox("HP Text", e ? &Globals::enemy_health_text : &Globals::team_health_text);
                }
                UI::Checkbox("Name", e ? &Globals::enemy_name : &Globals::team_name);
                if (*(e ? &Globals::enemy_name : &Globals::team_name)) {
                    UI::Color("Name Color", e ? Globals::esp_name_color : Globals::team_name_color);
                    static const char* npos[] = { "Top", "Bottom" };
                    UI::Combo("Name Pos", e ? &Globals::enemy_name_position : &Globals::team_name_position, npos, 2);
                }
                UI::Checkbox("Weapon", e ? &Globals::enemy_weapon : &Globals::team_weapon);
                if (*(e ? &Globals::enemy_weapon : &Globals::team_weapon)) {
                    static const char* wpos[] = { "Top", "Bottom" };
                    UI::Combo("Weapon Pos", e ? &Globals::enemy_weapon_position : &Globals::team_weapon_position, wpos, 2);
                    static const char* wmd[] = { "Text", "Icons" };
                    UI::Combo("Weapon Style", e ? &Globals::enemy_weapon_display_mode : &Globals::team_weapon_display_mode, wmd, 2);
                    UI::Checkbox("Ammo", e ? &Globals::enemy_ammo : &Globals::team_ammo);
                }
                UI::Checkbox("Distance", e ? &Globals::enemy_distance : &Globals::team_distance);
                UI::Checkbox("Armor", e ? &Globals::enemy_armor : &Globals::team_armor);
                UI::Checkbox("Flags", e ? &Globals::enemy_flags : &Globals::team_flags);
            }
            UI::EndCard();
        }

        ImGui::EndChild();

        ImGui::SameLine(0, gap2);

        // ── Right: ESP preview panel ────────────────────────────────────────────
        ImGui::BeginChild("##vis_R", ImVec2(rightW, 0), false, ImGuiWindowFlags_NoScrollbar);

        // Preview panel
        {
            ImDrawList* dL = ImGui::GetWindowDrawList();
            float panW = rightW - 2.f;
            float panH = 230.f;
            ImVec2 pa = ImGui::GetCursorScreenPos();
            ImVec2 pb = ImVec2(pa.x + panW, pa.y + panH);

            dL->AddRectFilled(pa, pb, IM_COL32(10, 10, 10, 255), 8.f);
            dL->AddRect(pa, pb, U32(Col::Border), 8.f);

            // Figure geometry
            float cx = pa.x + panW * 0.5f;
            float figH = panH * 0.62f;
            float figW = figH * 0.44f;
            float figX = cx - figW * 0.5f, figX2 = cx + figW * 0.5f;
            float figY = pa.y + panH * 0.15f, figY2 = figY + figH;

            // Helpers
            bool* chamsOn = e ? &Globals::enemy_chams_enabled : &Globals::team_chams_enabled;
            bool* boxOn = e ? &Globals::enemy_box : &Globals::team_box;
            bool* skelOn = e ? &Globals::enemy_skeleton : &Globals::team_skeleton;
            bool* glowOn = e ? &Globals::enemy_glow_enabled : &Globals::team_glow_enabled;
            bool* hpOn = e ? &Globals::enemy_health : &Globals::team_health;
            bool* nameOn = e ? &Globals::enemy_name : &Globals::team_name;
            bool* weapOn = e ? &Globals::enemy_weapon : &Globals::team_weapon;
            float* chamsC = e ? Globals::enemy_chams_visible_color : Globals::team_chams_visible_color;
            float* glowC = e ? Globals::enemy_glow_color : Globals::team_glow_color;
            float* boxC = e ? Globals::esp_box_color : Globals::esp_teammate_color;
            float* skelC = e ? Globals::esp_skeleton_color : Globals::team_skeleton_color;
            float* nameC = e ? Globals::esp_name_color : Globals::team_name_color;

            // Chams fill
            if (*chamsOn) {
                dL->AddRectFilled(ImVec2(figX, figY), ImVec2(figX2, figY2),
                    IM_COL32((int)(chamsC[0] * 255), (int)(chamsC[1] * 255), (int)(chamsC[2] * 255), 110), 2.f);
            }

            // Glow bloom
            if (*glowOn) {
                for (int gi = 4; gi >= 1; gi--) {
                    float g2 = gi * 2.5f;
                    int ga = 18 + (4 - gi) * 8;
                    dL->AddRect(ImVec2(figX - g2, figY - g2), ImVec2(figX2 + g2, figY2 + g2),
                        IM_COL32((int)(glowC[0] * 255), (int)(glowC[1] * 255), (int)(glowC[2] * 255), ga), 3.f, 0, 1.5f);
                }
            }

            // Box
            if (*boxOn) {
                ImU32 bc = IM_COL32((int)(boxC[0] * 255), (int)(boxC[1] * 255), (int)(boxC[2] * 255), (int)(boxC[3] * 255));
                dL->AddRect(ImVec2(figX, figY), ImVec2(figX2, figY2), bc, 0.f, 0, 1.5f);
                dL->AddRect(ImVec2(figX - 1, figY - 1), ImVec2(figX2 + 1, figY2 + 1), IM_COL32(0, 0, 0, 120));
            }

            // Skeleton
            if (*skelOn) {
                ImU32 sc2 = IM_COL32((int)(skelC[0] * 255), (int)(skelC[1] * 255), (int)(skelC[2] * 255), 220);
                float headR = figW * 0.175f;
                float headCy = figY + headR * 1.6f;
                dL->AddCircle(ImVec2(cx, headCy), headR, sc2, 14, 1.4f);
                float neckY = headCy + headR * 0.8f;
                float chestY = figY + figH * 0.36f;
                float waistY = figY + figH * 0.54f;
                float sw2 = figW * 0.38f;
                float kw = figW * 0.22f;
                float kneeY = figY + figH * 0.73f;
                // spine
                dL->AddLine(ImVec2(cx, neckY), ImVec2(cx, waistY), sc2, 1.4f);
                // shoulders + arms
                dL->AddLine(ImVec2(cx - sw2, chestY), ImVec2(cx + sw2, chestY), sc2, 1.4f);
                dL->AddLine(ImVec2(cx - sw2, chestY), ImVec2(cx - sw2 * 1.15f, waistY), sc2, 1.4f);
                dL->AddLine(ImVec2(cx + sw2, chestY), ImVec2(cx + sw2 * 1.15f, waistY), sc2, 1.4f);
                // legs
                dL->AddLine(ImVec2(cx, waistY), ImVec2(cx - kw, kneeY), sc2, 1.4f);
                dL->AddLine(ImVec2(cx, waistY), ImVec2(cx + kw, kneeY), sc2, 1.4f);
                dL->AddLine(ImVec2(cx - kw, kneeY), ImVec2(cx - kw, figY2), sc2, 1.4f);
                dL->AddLine(ImVec2(cx + kw, kneeY), ImVec2(cx + kw, figY2), sc2, 1.4f);
            }

            // Health bar
            if (*hpOn) {
                float hbX = figX - 7.f;
                float hbFrac = 0.72f;
                dL->AddRectFilled(ImVec2(hbX - 4.f, figY - 1.f), ImVec2(hbX - 1.f, figY2 + 1.f), IM_COL32(0, 0, 0, 160));
                float hbY0 = figY + figH * (1.f - hbFrac);
                dL->AddRectFilled(ImVec2(hbX - 3.f, hbY0), ImVec2(hbX - 2.f, figY2), IM_COL32(30, 220, 60, 255));
            }

            // Name
            if (*nameOn) {
                ImU32 nc2 = IM_COL32((int)(nameC[0] * 255), (int)(nameC[1] * 255), (int)(nameC[2] * 255), (int)(nameC[3] * 255));
                const char* nm = e ? "Enemy" : "Teammate";
                ImVec2 nt2 = ImGui::CalcTextSize(nm);
                float ny = figY - nt2.y - 4.f;
                if (ny > pa.y + 3.f)
                    dL->AddText(ImVec2(cx - nt2.x * 0.5f, ny), nc2, nm);
            }

            // Weapon
            if (*weapOn) {
                ImVec2 wt2 = ImGui::CalcTextSize("AK-47");
                float wy = figY2 + 5.f;
                if (wy + wt2.y < pb.y - 4.f)
                    dL->AddText(ImVec2(cx - wt2.x * 0.5f, wy), U32(Col::TextMid), "AK-47");
            }

            // "PREVIEW" watermark
            ImVec2 pwt = ImGui::CalcTextSize("PREVIEW");
            dL->AddText(ImVec2(pa.x + panW - pwt.x - 7.f, pb.y - pwt.y - 5.f), U32(Col::TextLo), "PREVIEW");

            ImGui::Dummy(ImVec2(panW, panH + 8.f));
        }

        ImGui::EndChild();
    }
    else { // World
        using namespace NecusState;
        ImGui::BeginChild("worldA", ImVec2(colW, 0), false, ImGuiWindowFlags_NoScrollbar);
        UI::ColTitle("World");
        UI::BeginCard("World ESP");
        {
            UI::Checkbox("Dropped Weapons", &Globals::world_dropped_weapons);
            UI::Checkbox("Grenades", &Globals::world_grenades);
            UI::Checkbox("Bomb (C4)", &Globals::world_c4);
            if (Globals::world_c4)
                UI::Checkbox("Bomb Timer", &Globals::world_c4_timer);
            UI::Checkbox("Grenade Trajectory", &Globals::grenade_trajectory);
        }
        UI::EndCard();

        UI::ColTitle("Effects");
        UI::BeginCard("Effects");
        {
            // Nightmode — color swatch + toggle (wires to existing Globals)
            UI::ColorToggle("Nightmode", g_worldfx.nm_col, &Globals::misc_nightmode);
            if (Globals::misc_nightmode)
                UI::SliderF("Brightness", &Globals::misc_nightmode_brightness, 0.05f, 1.0f, "%.2f");
            // Stub entries (visual only)
            UI::ColorToggle("Fullbright", g_worldfx.fb_col, &g_worldfx.fullbright);
            UI::ColorToggle("Override Sky", g_worldfx.sky_col, &g_worldfx.ovr_sky);
            UI::ColorToggle("Override Sunlight", g_worldfx.sun_col, &g_worldfx.ovr_sun);
            UI::ColorToggle("Override Fog", g_worldfx.fog_col, &g_worldfx.ovr_fog);
            UI::Checkbox("Override DoF", &g_worldfx.ovr_dof, &g_worldfx.kb_dof);
            // Weather expandable
            if (UI::ExpandRow("Weather", &g_worldfx.wx_open)) {
                static const char* wx[] = { "Clear", "Rain", "Snow", "Foggy" };
                ImGui::Indent(14.f);
                UI::Combo("##wx", &g_worldfx.weather, wx, 4);
                ImGui::Unindent(14.f);
            }
            // Removals dropdown
            {
                static const char* rm[] = { "Select", "Props", "Decals", "Both" };
                UI::Combo("Removals", &g_worldfx.removals, rm, 4);
            }
        }
        UI::EndCard();
        ImGui::EndChild();
        // (No Flash / No Smoke and the Equipments card moved to the Misc tab.)
    }
    ImGui::PopStyleColor();
}

static int g_skinsSub = 0; // 0 = Skins, 1 = Gloves

static void RenderSkinsTab() {
    // ── Sub-tab header ────────────────────────────────────────────────────────
    const char* skinSubs[] = { "Skins", "Gloves" };
    SegmentedSubTabs(skinSubs, 2, &g_skinsSub);

    // ── Build pointer arrays from item_schema vectors (capped at 512) ─────────
    // paint kits
    static const char* s_pkNames[512];
    static int         s_pkCount = 0;
    if (s_pkCount == 0) {
        s_pkCount = (int)paint_kits.size();
        if (s_pkCount > 512) s_pkCount = 512;
        for (int i = 0; i < s_pkCount; ++i)
            s_pkNames[i] = paint_kits[i].name;
    }

    // knife models
    static const char* s_knifeNames[64];
    static int         s_knifeCount = 0;
    if (s_knifeCount == 0) {
        s_knifeCount = (int)knife_items.size();
        if (s_knifeCount > 64) s_knifeCount = 64;
        for (int i = 0; i < s_knifeCount; ++i)
            s_knifeNames[i] = knife_items[i].name;
    }

    // glove models
    static const char* s_gloveNames[32];
    static int         s_gloveCount = 0;
    if (s_gloveCount == 0) {
        s_gloveCount = (int)glove_items.size();
        if (s_gloveCount > 32) s_gloveCount = 32;
        for (int i = 0; i < s_gloveCount; ++i)
            s_gloveNames[i] = glove_items[i].name;
    }

    // Helper: return condition tier string for a float wear value
    auto WearTier = [](float w) -> const char* {
        if (w < 0.07f) return "FN";
        if (w < 0.15f) return "MW";
        if (w < 0.38f) return "FT";
        if (w < 0.45f) return "WW";
        return "BS";
        };

    float gap = 14.f, colW = (ImGui::GetContentRegionAvail().x - gap) / 2.f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

    // ══════════════════════════════════════════════════════════════════════════
    //  Sub-tab 0 : Skins
    // ══════════════════════════════════════════════════════════════════════════
    if (g_skinsSub == 0) {
        // ── Left column: Skin changer ─────────────────────────────────────────
        ImGui::BeginChild("skinsL", ImVec2(colW, 0), false, ImGuiWindowFlags_NoScrollbar);
        UI::ColTitle("Skin Changer");
        UI::BeginCard("Skin changer");
        {
            UI::Checkbox("Enabled", &Globals::skin_changer_enabled);

            // Searchable combo for paint kit
            static char s_skinPkSearch[64] = {};
            ImGui::SetNextItemWidth(-1.f);
            ImGui::InputText("##skinpksearch", s_skinPkSearch, sizeof(s_skinPkSearch));
            // Build filtered list
            static const char* s_filtered[512];
            static int         s_filtIdx[512];
            int filtCount = 0;
            for (int i = 0; i < s_pkCount && filtCount < 512; ++i) {
                if (s_skinPkSearch[0] == '\0' ||
                    ImGui::GetIO().KeyCtrl ||
                    strstr(s_pkNames[i], s_skinPkSearch) != nullptr)
                {
                    s_filtered[filtCount] = s_pkNames[i];
                    s_filtIdx[filtCount++] = i;
                }
            }
            // Find current selection index in filtered list
            int selInFiltered = 0;
            for (int i = 0; i < filtCount; ++i) {
                if (s_filtIdx[i] == Globals::skin_changer_paint_kit) {
                    selInFiltered = i;
                    break;
                }
            }
            if (UI::Combo("Paint kit##skinpk", &selInFiltered, s_filtered, filtCount))
                Globals::skin_changer_paint_kit = s_filtIdx[selInFiltered];

            // Wear slider with condition tier
            char wearLabel[32];
            snprintf(wearLabel, sizeof(wearLabel), "Wear  [%s]", WearTier(Globals::skin_changer_wear));
            UI::SliderF(wearLabel, &Globals::skin_changer_wear, 0.0f, 1.0f);

            ImGui::SetNextItemWidth(-1.f);
            ImGui::InputInt("Seed##skinseed", &Globals::skin_changer_seed);
            ImGui::SetNextItemWidth(-1.f);
            ImGui::InputText("Custom name##skincname", Globals::skin_changer_custom_name, 161);
        }
        UI::EndCard();
        ImGui::EndChild();

        ImGui::SameLine(0, gap);

        // ── Right column: Knife ───────────────────────────────────────────────
        ImGui::BeginChild("skinsR", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
        UI::ColTitle("Knife");
        UI::BeginCard("Knife");
        {
            UI::Checkbox("Enabled", &Globals::knife_changer_enabled);
            UI::Combo("Model##knifemodel", &Globals::knife_changer_model, s_knifeNames, s_knifeCount);
            UI::Combo("Paint##knifepk", &Globals::knife_changer_paint_kit, s_pkNames, s_pkCount);

            char wearLabel[32];
            snprintf(wearLabel, sizeof(wearLabel), "Wear  [%s]", WearTier(Globals::knife_changer_wear));
            UI::SliderF(wearLabel, &Globals::knife_changer_wear, 0.0f, 1.0f);

            ImGui::SetNextItemWidth(-1.f);
            ImGui::InputInt("Seed##knifeseed", &Globals::knife_changer_seed);
        }
        UI::EndCard();
        ImGui::EndChild();
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Sub-tab 1 : Gloves
    // ══════════════════════════════════════════════════════════════════════════
    else if (g_skinsSub == 1) {
        ImGui::BeginChild("glovesL", ImVec2(colW, 0), false, ImGuiWindowFlags_NoScrollbar);
        UI::ColTitle("Gloves");
        UI::BeginCard("Gloves");
        {
            UI::Checkbox("Enabled", &Globals::glove_changer_enabled);
            UI::Combo("Model##glovemodel", &Globals::glove_changer_model, s_gloveNames, s_gloveCount);
            UI::Combo("Paint##glovepk", &Globals::glove_changer_paint_kit, s_pkNames, s_pkCount);

            char wearLabel[32];
            snprintf(wearLabel, sizeof(wearLabel), "Wear  [%s]", WearTier(Globals::glove_changer_wear));
            UI::SliderF(wearLabel, &Globals::glove_changer_wear, 0.0f, 1.0f);

            ImGui::SetNextItemWidth(-1.f);
            ImGui::InputInt("Seed##gloveseed", &Globals::glove_changer_seed);
        }
        UI::EndCard();
        ImGui::EndChild();
    }

    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  OVERLAYS  (always rendered — not gated by IsOpen)
// ─────────────────────────────────────────────────────────────────────────────
static uintptr_t OvSafeRead(uintptr_t addr) {
    __try { return *reinterpret_cast<uintptr_t*>(addr); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static uintptr_t OvGetEnt(uintptr_t listPtr, int i) {
    __try {
        uintptr_t chunk = OvSafeRead(listPtr + 8 * ((i & 0x7FFF) >> 9) + 16);
        if (!chunk) return 0;
        return OvSafeRead(chunk + 120 * (i & 0x1FF));
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// ── Draggable indicators ──────────────────────────────────────────────────────
// While the menu is open every overlay gets a subtle accent border and can be
// moved with LMB drag. Manual implementation: on LMB press inside the rect we
// record the grab offset relative to the stored anchor; while LMB is held the
// anchor follows the mouse. Only one indicator can be dragged at a time.
static char   g_dragActive[24] = {};
static ImVec2 g_dragGrab(0, 0);

static void IndicatorDrag(const char* id, float* pos, const ImVec2& mn, const ImVec2& mx) {
    if (!Menu::IsOpen) {
        if (g_dragActive[0] && strcmp(g_dragActive, id) == 0) g_dragActive[0] = '\0';
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    // subtle border to show the indicator is draggable
    ImGui::GetForegroundDrawList()->AddRect(mn, mx, U32(Col::AccentGl), 7.f, 0, 1.f);

    bool lmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool active = g_dragActive[0] && strcmp(g_dragActive, id) == 0;

    if (!lmbDown) {
        if (active) g_dragActive[0] = '\0';
        return;
    }
    if (!active && g_dragActive[0] == '\0' && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        bool inside = io.MousePos.x >= mn.x && io.MousePos.x <= mx.x &&
            io.MousePos.y >= mn.y && io.MousePos.y <= mx.y;
        // don't steal the click when an ImGui window (the menu) is under the mouse
        if (inside && !io.WantCaptureMouse) {
            strncpy_s(g_dragActive, id, _TRUNCATE);
            g_dragGrab = ImVec2(io.MousePos.x - pos[0], io.MousePos.y - pos[1]);
            active = true;
        }
    }
    if (active) {
        pos[0] = io.MousePos.x - g_dragGrab.x;
        pos[1] = io.MousePos.y - g_dragGrab.y;
        // keep on screen
        if (pos[0] < 0.f) pos[0] = 0.f;
        if (pos[1] < 0.f) pos[1] = 0.f;
        if (pos[0] > io.DisplaySize.x - 20.f) pos[0] = io.DisplaySize.x - 20.f;
        if (pos[1] > io.DisplaySize.y - 20.f) pos[1] = io.DisplaySize.y - 20.f;
    }
}

static void RenderWatermark() {
    if (!Globals::misc_watermark) return;
    ImGuiIO& io = ImGui::GetIO();
    char buf[64];
    snprintf(buf, sizeof(buf), "  |  %.0f fps", io.Framerate);
    ImVec2 ns = ImGui::CalcTextSize("Necus");
    ImVec2 rs = ImGui::CalcTextSize(buf);
    float totalW = ns.x + rs.x;
    float th = ImGui::GetTextLineHeight();
    float padX = 12.f, padY = 7.f;
    float x = Globals::ind_watermark_pos[0], y = Globals::ind_watermark_pos[1];
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 mn(x - padX, y - padY), mx(x + totalW + padX, y + th + padY);
    dl->AddRectFilled(mn, mx, IM_COL32(17, 17, 17, 215), 7.f);
    dl->AddRect(mn, mx, U32(Col::Border), 7.f, 0, 1.f);
    dl->AddText(ImVec2(x, y), U32(Col::Accent), "Necus");
    dl->AddText(ImVec2(x + ns.x, y), U32(Col::TextMid), buf);
    IndicatorDrag("watermark", Globals::ind_watermark_pos, mn, mx);
}

// Damage log overlay: last 5 entries, fading out after 5 seconds.
static void RenderDamageLog() {
    if (!Globals::misc_damage_logs) return;

    Visuals::DmgEntry entries[5];
    int n = Visuals::GetDamageLog(entries, 5);
    if (n == 0) return;

    ImGuiIO& io = ImGui::GetIO();
    if (Globals::ind_dmglog_pos[0] < 0.f) {                 // default: top-right
        Globals::ind_dmglog_pos[0] = io.DisplaySize.x - 280.f;
        Globals::ind_dmglog_pos[1] = 12.f;
    }
    float x = Globals::ind_dmglog_pos[0], y = Globals::ind_dmglog_pos[1];

    double now = ImGui::GetTime();
    float lh = ImGui::GetTextLineHeight() + 5.f;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    float maxW = 0.f;
    char lines[5][96];
    for (int i = 0; i < n; ++i) {
        snprintf(lines[i], sizeof(lines[i]), "Hit %s for %d (%d hp)",
            entries[i].name, entries[i].dmg, entries[i].hp);
        float w = ImGui::CalcTextSize(lines[i]).x;
        if (w > maxW) maxW = w;
    }
    ImVec2 mn(x - 10.f, y - 7.f), mx(x + maxW + 12.f, y + lh * n + 7.f);
    dl->AddRectFilled(mn, mx, IM_COL32(17, 17, 17, 180), 7.f);
    dl->AddRect(mn, mx, U32(Col::Border), 7.f, 0, 1.f);
    for (int i = 0; i < n; ++i) {
        float age = (float)(now - entries[i].time);
        float alpha = age > 4.f ? (5.f - age) : 1.f;        // fade over the last second
        if (alpha < 0.f) alpha = 0.f;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 1.f, 1.f, 0.85f * alpha));
        dl->AddText(ImVec2(x, y + i * lh), col, lines[i]);
    }
    IndicatorDrag("dmglog", Globals::ind_dmglog_pos, mn, mx);
}

static void RenderKeybindList() {
    if (!Globals::misc_show_keybind_list) return;

    struct Entry { std::string label; std::string keys; bool active; };
    std::vector<Entry> entries;
    for (auto& [lbl, bs] : g_binds) {
        if (bs.vks.empty()) continue;
        std::string keys;
        for (int i = 0; i < (int)bs.vks.size(); ++i) {
            if (i) keys += '+';
            keys += VkName(bs.vks[i]);
        }
        entries.push_back({ lbl, keys, bs.active });
    }
    if (entries.empty()) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    float lh = ImGui::GetTextLineHeight() + 5.f;
    float maxW = 0.f;
    for (auto& e : entries) {
        char tmp[128]; snprintf(tmp, sizeof(tmp), "%s  [%s]", e.label.c_str(), e.keys.c_str());
        float w = ImGui::CalcTextSize(tmp).x;
        if (w > maxW) maxW = w;
    }
    maxW += 22.f;
    if (Globals::ind_keybind_pos[0] < 0.f) {
        Globals::ind_keybind_pos[0] = io.DisplaySize.x - maxW - 14.f;
        Globals::ind_keybind_pos[1] = io.DisplaySize.y - lh * entries.size() - 22.f;
    }
    float x = Globals::ind_keybind_pos[0];
    float y = Globals::ind_keybind_pos[1];
    ImVec2 mn(x - 10, y - 8), mx(x + maxW, y + lh * entries.size() + 10);
    dl->AddRectFilled(mn, mx, IM_COL32(17, 17, 17, 205), 7.f);
    dl->AddRect(mn, mx, U32(Col::Border), 7.f, 0, 1.f);
    for (int i = 0; i < (int)entries.size(); ++i) {
        float ly = y + i * lh;
        char kbuf[32]; snprintf(kbuf, sizeof(kbuf), "[%s]", entries[i].keys.c_str());
        float kw = ImGui::CalcTextSize(kbuf).x;
        ImU32 lc = entries[i].active ? U32(Col::TextHi) : U32(ImVec4(1, 1, 1, 0.45f));
        ImU32 kc = entries[i].active ? U32(Col::Accent) : U32(Col::TextMid);
        dl->AddText(ImVec2(x, ly), lc, entries[i].label.c_str());
        dl->AddText(ImVec2(x + maxW - kw - 10.f, ly), kc, kbuf);
    }
    IndicatorDrag("keybinds", Globals::ind_keybind_pos, mn, mx);
}

static void RenderSpectatorList() {
    if (!Globals::misc_spectator_list) return;
    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;
    __try {
        uintptr_t listPtr = OvSafeRead(client + Offsets::dwEntityList);
        if (!listPtr) return;
        uintptr_t localCtrl = OvSafeRead(client + Offsets::dwLocalPlayerController);
        if (!localCtrl) return;
        int32_t localPawnHandle = *reinterpret_cast<int32_t*>(localCtrl + Offsets::m_hPlayerPawn);

        char names[20][64] = {};
        int count = 0;
        for (int i = 1; i < 64 && count < 20; ++i) {
            __try {
                uintptr_t ctrl = OvGetEnt(listPtr, i);
                if (!ctrl || ctrl == localCtrl) continue;
                int32_t ph = *reinterpret_cast<int32_t*>(ctrl + Offsets::m_hPlayerPawn);
                if (ph == -1 || ph == 0) continue;
                uintptr_t pawnChunk = OvSafeRead(listPtr + 8 * ((ph & 0x7FFF) >> 9) + 16);
                if (!pawnChunk) continue;
                uintptr_t pawn = OvSafeRead(pawnChunk + 120 * (ph & 0x1FF));
                if (!pawn) continue;
                uintptr_t obsSvc = OvSafeRead(pawn + Offsets::m_pObserverServices);
                if (!obsSvc) continue;
                int obsMode = *reinterpret_cast<int*>(obsSvc + Offsets::m_iObserverMode);
                if (obsMode == 0) continue;
                int32_t target = *reinterpret_cast<int32_t*>(obsSvc + Offsets::m_hObserverTarget);
                if (target != localPawnHandle) continue;
                const char* nm = reinterpret_cast<const char*>(ctrl + Offsets::m_iszPlayerName);
                strncpy_s(names[count++], nm, 63);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        }
        if (count == 0) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        float lh = ImGui::GetTextLineHeight() + 4.f;
        float tlh = lh + 4.f;
        float maxW = ImGui::CalcTextSize("Spectators (00)").x;
        for (int i = 0; i < count; ++i) {
            float w = ImGui::CalcTextSize(names[i]).x;
            if (w > maxW) maxW = w;
        }
        maxW += 24.f;
        float x = Globals::ind_spec_pos[0], y = Globals::ind_spec_pos[1];
        float boxH = tlh + lh * count + 10.f;
        ImVec2 mn(x - 10, y - 6), mx(x + maxW, y + boxH);
        dl->AddRectFilled(mn, mx, IM_COL32(17, 17, 17, 205), 7.f);
        dl->AddRect(mn, mx, U32(Col::Border), 7.f, 0, 1.f);
        char title[32]; snprintf(title, sizeof(title), "Spectators (%d)", count);
        dl->AddText(ImVec2(x, y), U32(Col::Accent), title);
        y += tlh;
        for (int i = 0; i < count; ++i)
            dl->AddText(ImVec2(x, y + i * lh), U32(Col::TextMid), names[i]);
        IndicatorDrag("spectators", Globals::ind_spec_pos, mn, mx);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void Menu::RenderOverlays() {
    if (g_necusFont) ImGui::PushFont(g_necusFont);
    RenderWatermark();
    RenderKeybindList();
    RenderSpectatorList();
    RenderDamageLog();
    if (g_necusFont) ImGui::PopFont();
}

static void RenderMiscTab() {
    float gap = 14.f;
    float colW = SC::ColW(gap);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

    // ── Left column: Viewmodel + Movement ────────────────────────────────────
    SC::Begin("##miscL", ImVec2(colW, 0));

    UI::ColTitle("Viewmodel");
    UI::BeginCard("Viewmodel Editor");
    {
        UI::Checkbox("Enable", &Globals::viewmodel_enabled);
        if (Globals::viewmodel_enabled) {
            UI::SliderF("X", &Globals::viewmodel_x, -10.f, 10.f, "%.1f");
            UI::SliderF("Y", &Globals::viewmodel_y, -10.f, 10.f, "%.1f");
            UI::SliderF("Z", &Globals::viewmodel_z, -10.f, 10.f, "%.1f");
            UI::SliderF("FOV", &Globals::viewmodel_fov, 54.f, 130.f, "%.0f");
        }
    }
    UI::EndCard();

    UI::ColTitle("Movement");
    UI::BeginCard("Movement");
    {
        UI::Checkbox("BunnyHop", &Globals::misc_bhop);
        if (Globals::misc_bhop) {
            static const char* bhm[] = { "Rage", "Legit" };
            UI::Combo("Mode##bh", &Globals::misc_bhop_mode, bhm, 2);
            if (Globals::misc_bhop_mode == 1)
                UI::SliderI("Chance##bh", &Globals::misc_bhop_chance, 50, 100, "%.0f%%");
        }
        UI::Checkbox("Auto Strafer", &Globals::misc_autostrafer);
        if (Globals::misc_autostrafer)
            UI::SliderF("Smooth##str", &Globals::misc_autostrafer_smooth, 0.1f, 3.0f, "%.1f");
        UI::Checkbox("Infinite Duck", &Globals::misc_infinite_duck);
    }
    UI::EndCard();

    UI::ColTitle("Visuals");
    UI::BeginCard("Helpers");
    {
        UI::Checkbox("Sniper Crosshair", &Globals::misc_sniper_crosshair);
        UI::Checkbox("2D Radar", &Globals::misc_radar_2d);
        UI::Checkbox("Damage Logs", &Globals::misc_damage_logs);
        UI::Checkbox("Reveal Ranks", &Globals::misc_reveal_ranks);

        UI::Checkbox("Show Equipments", &Globals::misc_show_money);
        if (Globals::misc_show_money) {
            static const char* pi[] = { "Armor", "Defuse Kit", "Zeus", "Pistol", "Primary" };
            UI::MultiCombo("Player Info", pi, Globals::misc_player_info, 5);
        }
    }
    UI::EndCard();

    SC::End();

    SC::SameLine(gap);

    // ── Right column: Config (top) + Misc ────────────────────────────────────
    SC::Begin("##miscR", ImVec2(0, 0));

    UI::ColTitle("Configuration");
    UI::BeginCard("Configuration");
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col::Frame);
        ImGui::SetNextItemWidth(-UI::kPadX);
        ImGui::InputTextWithHint("##cfgname", "Config name", g_cfgName, IM_ARRAYSIZE(g_cfgName));
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 4));
        float bw = (ImGui::GetContentRegionAvail().x - UI::kPadX - 14) / 3.f;
        if (UI::StyledButton("Save", ImVec2(bw, 30), true)) Config::Save(g_cfgName);
        ImGui::SameLine(0, 7);
        if (UI::StyledButton("Load", ImVec2(bw, 30)))       Config::Load(g_cfgName);
        ImGui::SameLine(0, 7);
        if (UI::StyledButton("Delete", ImVec2(bw, 30))) {
            Config::DeleteConfig(g_cfgName);
            memset(g_cfgName, 0, sizeof(g_cfgName));
        }
        auto configs = Config::ListConfigs();
        if (!configs.empty()) {
            ImGui::Dummy(ImVec2(0, 6));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Col::AccentDim);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, Col::AccentGl);
            for (const auto& name : configs)
                if (ImGui::Selectable(name.c_str()))
                    strncpy_s(g_cfgName, name.c_str(), sizeof(g_cfgName) - 1);
            ImGui::PopStyleColor(2);
        }
    }
    UI::EndCard();

    UI::ColTitle("Misc");
    UI::BeginCard("Misc");
    {
        UI::Checkbox("Auto Accept", &Globals::misc_auto_accept);

        {
            static bool s_indOpen = false;
            if (UI::ExpandRow("Indicators", &s_indOpen)) {
                ImGui::Indent(14.f);
                UI::Checkbox("Watermark", &Globals::misc_watermark);
                UI::Checkbox("Spectator List", &Globals::misc_spectator_list);
                UI::Checkbox("Keybind List", &Globals::misc_show_keybind_list);
                ImGui::Unindent(14.f);
            }
        }

        UI::Checkbox("Thirdperson", &Globals::misc_thirdperson);
        if (Globals::misc_thirdperson)
            UI::SliderF("Distance##tp", &Globals::misc_thirdperson_dist, 50.f, 300.f, "%.0f");

        UI::Checkbox("Aspect Ratio", &Globals::misc_aspect_ratio_enabled);
        if (Globals::misc_aspect_ratio_enabled)
            UI::SliderF("Ratio##ar", &Globals::misc_aspect_ratio, 0.5f, 2.5f, "%.2f");

        UI::Checkbox("Spread circle", &Globals::misc_spread_circle);
        UI::Checkbox("Penetration crosshair", &Globals::misc_penetration_xhair);
        if (Globals::misc_penetration_xhair) {
            UI::Color("No pen color", Globals::misc_pen_no_color);
            UI::Color("Can pen color", Globals::misc_pen_yes_color);
        }

        UI::Checkbox("No Flash", &Globals::misc_noflash);
        UI::Checkbox("No Smoke", &Globals::misc_nosmoke);

        {
            static const char* remOpts[] = { "Crosshair", "Scope overlay", "Legs", "Shadows", "Team intro" };
            bool remFlags[5] = { Globals::misc_remove_crosshair,    Globals::misc_remove_scope_overlay,
                                 Globals::misc_remove_legs,         Globals::misc_remove_shadows,
                                 Globals::misc_remove_team_intro };
            UI::MultiCombo("Removals", remOpts, remFlags, 5);
            Globals::misc_remove_crosshair = remFlags[0];
            Globals::misc_remove_scope_overlay = remFlags[1];
            Globals::misc_remove_legs = remFlags[2];
            Globals::misc_remove_shadows = remFlags[3];
            Globals::misc_remove_team_intro = remFlags[4];
        }

        UI::Checkbox("Hit marker", &Globals::misc_hit_marker);
        if (Globals::misc_hit_marker) {
            static const char* trOpts[] = { "Teammates", "Enemies" };
            bool trFlags[2] = { Globals::misc_hit_marker_tracer_team, Globals::misc_hit_marker_tracer_enemy };
            UI::MultiCombo("Tracers##hm", trOpts, trFlags, 2);
            Globals::misc_hit_marker_tracer_team = trFlags[0];
            Globals::misc_hit_marker_tracer_enemy = trFlags[1];
            UI::Color("Color##hm", Globals::misc_hit_marker_color);
            UI::SliderF("Size##hm", &Globals::misc_hit_marker_size, 2.f, 24.f, "%.0fpx");
            UI::SliderF("Duration##hm", &Globals::misc_hit_marker_duration, 0.1f, 2.f, "%.1fs");
        }
    }
    UI::EndCard();

    SC::End();
    ImGui::PopStyleColor();
}

// (RenderConfigTab / RenderScriptsTab removed — the reference has only 5 tabs;
//  configuration now lives inside the Misc tab above.)

// ─────────────────────────────────────────────────────────────────────────────
//  FONT  (call ONCE at init — fixes the blurry default ImGui font)
//  Poppins is now COMPILED IN (see PoppinsFont.h) — no external .ttf required:
//      NecusLoadFontEmbedded(17.0f);
//  Call after ImGui::CreateContext() and BEFORE your first ImGui::NewFrame().
//  If you rebuild fonts at runtime, also call ImGui_ImplDX11_InvalidateDeviceObjects().
//  NecusLoadFont(path,...) is kept as a fallback if you'd rather load from a file.
//  (Add the prototypes to Menu.h if you call these from another translation unit.)
// ─────────────────────────────────────────────────────────────────────────────
// Embedded Poppins — matches the gamesense v2 reference, needs no external file.
ImFont* NecusLoadFontEmbedded(float sizePx) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;   // static array in PoppinsFont.h — ImGui must NOT free it
    cfg.OversampleH = 3;              // crisp horizontal rasterization
    cfg.OversampleV = 2;
    cfg.PixelSnapH = false;
    cfg.RasterizerMultiply = 1.15f;     // slightly heavier strokes -> less washed out
    g_necusFont = io.Fonts->AddFontFromMemoryTTF(
        (void*)g_poppins_ttf, (int)g_poppins_ttf_len, sizePx, &cfg,
        io.Fonts->GetGlyphRangesCyrillic());   // Latin + Cyrillic
    io.Fonts->Build();
    return g_necusFont;
}

// Fallback: load Poppins (or any TTF) from a file path.
ImFont* NecusLoadFont(const char* ttfPath, float sizePx) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.OversampleH = 3;       // crisp horizontal rasterization
    cfg.OversampleV = 2;
    cfg.PixelSnapH = false;
    cfg.RasterizerMultiply = 1.15f;   // slightly heavier strokes -> less washed out
    g_necusFont = io.Fonts->AddFontFromFileTTF(ttfPath, sizePx, &cfg,
        io.Fonts->GetGlyphRangesCyrillic());   // Latin + Cyrillic
    io.Fonts->Build();
    return g_necusFont;
}

// ─────────────────────────────────────────────────────────────────────────────
//  GLOBAL STYLE
// ─────────────────────────────────────────────────────────────────────────────
static void ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 12.f;
    s.ChildRounding = 8.f;
    s.FrameRounding = 6.f;
    s.PopupRounding = 8.f;
    s.GrabRounding = 6.f;
    s.ScrollbarRounding = 6.f;
    s.ScrollbarSize = 6.f;
    s.FrameBorderSize = 1.f;
    s.WindowBorderSize = 1.f;
    s.PopupBorderSize = 1.f;
    s.ItemSpacing = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 6);
    s.FramePadding = ImVec2(10, 6);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = Col::TextHi;
    c[ImGuiCol_TextDisabled] = Col::TextLo;
    c[ImGuiCol_WindowBg] = Col::Window;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = ImVec4(0.118f, 0.118f, 0.118f, 1.f);
    c[ImGuiCol_Border] = Col::Border;
    c[ImGuiCol_FrameBg] = Col::Frame;
    c[ImGuiCol_FrameBgHovered] = Col::FrameHi;
    c[ImGuiCol_FrameBgActive] = Col::FrameHi;
    c[ImGuiCol_Button] = Col::Frame;
    c[ImGuiCol_ButtonHovered] = Col::FrameHi;
    c[ImGuiCol_ButtonActive] = Col::FrameHi;
    c[ImGuiCol_Header] = Col::AccentDim;
    c[ImGuiCol_HeaderHovered] = Col::AccentDim;
    c[ImGuiCol_HeaderActive] = Col::AccentGl;
    c[ImGuiCol_CheckMark] = Col::Accent;
    c[ImGuiCol_SliderGrab] = Col::Accent;
    c[ImGuiCol_SliderGrabActive] = Col::AccentHi;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = Col::Border2;
    c[ImGuiCol_ScrollbarGrabHovered] = Col::Accent;
    c[ImGuiCol_Separator] = Col::Border;
}

// ─────────────────────────────────────────────────────────────────────────────
//  MAIN RENDER
// ─────────────────────────────────────────────────────────────────────────────
void Menu::Render()
{
    static int activeTab = 0;                              // 0 = Rage
    const int    tabCount = 5;   // match the gamesense v2 reference (Config folded into Misc)

    static bool styled = false;
    if (!styled) { ApplyStyle(); styled = true; }   // apply theme once

    // RMB edge detection — used by UI::Checkbox to open the bind popup.
    // GetAsyncKeyState alone samples once per frame and MISSES fast clicks that
    // press+release between two frames, which made repeated RMB on the same row
    // appear to "not reopen" the popup. ImGui's event queue never drops clicks,
    // so OR both sources.
    {
        bool rmbNow = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        g_rmbJustClicked = (rmbNow && !g_rmbWasDown)
            || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        g_rmbWasDown = rmbNow;
    }
    UpdateKeybindCapture();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(760, 600), ImGuiCond_Once);
    ImGui::Begin("Necus", &IsOpen,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    if (g_necusFont) ImGui::PushFont(g_necusFont);   // draw the whole menu in embedded Poppins

    ImVec2 winSize = ImGui::GetWindowSize();
    float fullH = winSize.y;

    // ── SIDEBAR ──────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Col::Sidebar);
    ImGui::BeginChild("##sidebar", ImVec2(66, fullH), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 base = ImGui::GetWindowPos();

        // logo
        ImVec2 lc(base.x + 33, base.y + 32);
        if (g_logoTex) {
            dl->AddImage(g_logoTex, ImVec2(lc.x - 18, lc.y - 18), ImVec2(lc.x + 18, lc.y + 18));
        }
        else {
            // fallback: orange emblem (two overlapping blobs ~ the swirl logo)
            dl->AddCircleFilled(ImVec2(lc.x - 3, lc.y - 3), 11.f, U32(Col::Accent));
            dl->AddCircleFilled(ImVec2(lc.x + 6, lc.y + 6), 6.f, U32(ImVec4(0.831f, 0.357f, 0.071f, 1.f)));
            dl->AddCircleFilled(ImVec2(lc.x - 5, lc.y + 5), 3.5f, U32(Col::AccentHi));
        }

        ImGui::SetCursorPosY(70);
        for (int i = 0; i < tabCount; ++i) {
            ImGui::SetCursorPosX(10);
            ImGui::PushID(i);
            bool active = (activeTab == i);
            ImVec2 cp = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("tab", ImVec2(46, 46));
            bool hov = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) activeTab = i;
            ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
            if (active)      dl->AddRectFilled(mn, mx, U32(Col::AccentDim), 10.f);
            else if (hov)    dl->AddRectFilled(mn, mx, U32(ImVec4(1, 1, 1, 0.035f)), 10.f);
            ImU32 ic = active ? U32(Col::Accent) : U32(hov ? ImVec4(1, 1, 1, 0.7f) : Col::TextMid);
            DrawTabIcon(dl, ImVec2((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f), i, ic);
            if (active) dl->AddRectFilled(ImVec2(mn.x - 10, mn.y + 11), ImVec2(mn.x - 7, mx.y - 11), U32(Col::Accent), 2.f);
            ImGui::PopID();
            ImGui::Dummy(ImVec2(0, 6));
        }

        // profile pinned to bottom
        ImVec2 pc(base.x + 33, base.y + fullH - 34);
        dl->AddRectFilled(ImVec2(pc.x - 19, pc.y - 19), ImVec2(pc.x + 19, pc.y + 19), U32(ImVec4(0.14f, 0.10f, 0.13f, 1.f)), 9.f);
        dl->AddRect(ImVec2(pc.x - 19, pc.y - 19), ImVec2(pc.x + 19, pc.y + 19), U32(Col::Border2), 9.f, 0, 1.f);
        dl->AddText(ImVec2(pc.x - ImGui::CalcTextSize("S").x * 0.5f, pc.y - 7), U32(Col::TextHi), "S");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 0);

    // ── MAIN AREA ────────────────────────────────────────────────────────────
    ImGui::BeginGroup();
    {
        // Header
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Col::Header);
        ImGui::BeginChild("##header", ImVec2(0, 54), false, ImGuiWindowFlags_NoScrollbar);
        {
            ImDrawList* hdl = ImGui::GetWindowDrawList();

            // Weapon picker — only on Rage & Legit (both are per-weapon aimbots)
            bool showPicker = (activeTab == 0 || activeTab == 1);
            int* wIdx = (activeTab == 1) ? &NecusState::g_legitWeapon : &NecusState::g_rageWeapon;
            ImVec2 wMn(0, 0), wMx(0, 0);
            if (showPicker) {
                ImGui::SetCursorPos(ImVec2(16, 11));
                ImGui::InvisibleButton("wpick", ImVec2(200, 32));
                bool hov = ImGui::IsItemHovered();
                bool open = ImGui::IsPopupOpen("##weaponpick");
                if (ImGui::IsItemClicked()) ImGui::OpenPopup("##weaponpick");
                wMn = ImGui::GetItemRectMin(); wMx = ImGui::GetItemRectMax();
                float cy = (wMn.y + wMx.y) * 0.5f;
                hdl->AddRectFilled(wMn, wMx, U32(Col::Frame), 7.f);
                hdl->AddRect(wMn, wMx, U32((open || hov) ? Col::Accent : Col::Border), 7.f, 0, 1.f);
                DrawWeaponIcon(hdl, ImVec2(wMn.x + 16, cy), U32((open || hov) ? Col::Accent : Col::TextMid));
                hdl->AddText(ImVec2(wMn.x + 33, cy - ImGui::GetTextLineHeight() * 0.5f), U32(Col::TextHi), NecusState::kWeapons[*wIdx]);
                float ax = wMx.x - 16, ay = cy;
                hdl->AddLine(ImVec2(ax - 4, ay - 2), ImVec2(ax, ay + 2), U32(Col::TextMid), 1.4f);
                hdl->AddLine(ImVec2(ax, ay + 2), ImVec2(ax + 4, ay - 2), U32(Col::TextMid), 1.4f);
            }

            // "Binds" button (right-aligned, before search)
            float searchW = 180.f;
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - searchW - 16 - 70);
            ImGui::SetCursorPosY(13);
            ImGui::PushStyleColor(ImGuiCol_Button, g_showAllBinds ? Col::AccentDim : Col::Frame);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::AccentGl);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::AccentGl);
            ImGui::PushStyleColor(ImGuiCol_Text, g_showAllBinds ? Col::Accent : Col::TextMid);
            if (ImGui::Button("Binds##hdr", ImVec2(62, 28))) g_showAllBinds = !g_showAllBinds;
            ImGui::PopStyleColor(4);
            ImGui::SameLine(0, 8);

            // Search (right-aligned)
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - searchW - 16);
            ImGui::SetCursorPosY(11);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, Col::Frame);
            ImGui::SetNextItemWidth(searchW);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 7));
            if (ImGui::InputTextWithHint("##search", "Search", g_SearchBuf, sizeof(g_SearchBuf)))
                g_SearchActive = (g_SearchBuf[0] != '\0');
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            // Weapon popup — styled to match the combo dropdowns (orange dot, hover, scroll)
            if (showPicker) {
                if (ImGui::IsPopupOpen("##weaponpick")) {
                    ImGui::SetNextWindowPos(ImVec2(wMn.x, wMx.y + 4));
                    ImGui::SetNextWindowSizeConstraints(ImVec2(wMx.x - wMn.x, 0), ImVec2(wMx.x - wMn.x, 300));
                }
                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.118f, 0.118f, 0.118f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_Border, Col::Border2);
                ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 7.f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
                if (ImGui::BeginPopup("##weaponpick")) {
                    float pw = ImGui::GetContentRegionAvail().x;
                    for (int i = 0; i < NecusState::kWeaponCount; ++i)
                        if (UI::OptionRow(NecusState::kWeapons[i], *wIdx == i, false, pw)) {
                            *wIdx = i;
                            ImGui::CloseCurrentPopup();
                        }
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Body
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Col::Content);
        ImGui::BeginChild("##body", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
        {
            ImGui::Dummy(ImVec2(0, 12));
            ImGui::Indent(16);
            // column titles ("Aimbot", "Anti-Aim", ...) act as the section headers,
            // exactly like the gamesense v2 reference — no extra tab-name heading.
            ImGui::PushItemWidth(ImGui::GetWindowWidth() - 32);
            ImGui::BeginChild("##bodyinner", ImVec2(ImGui::GetWindowWidth() - 32, ImGui::GetContentRegionAvail().y - 6), false, ImGuiWindowFlags_NoScrollbar);
            if (g_SearchActive) {
                // Global search: render ALL tabs' content (search filter hides non-matching cards)
                // Use g_searchMode=true so BeginChild/EndChild/SameLine wrappers become no-ops
                g_searchMode = true;
                RenderRageTab();
                RenderLegitTab();
                RenderVisualsTab();
                RenderSkinsTab();
                RenderMiscTab();
                g_searchMode = false;
            }
            else {
                switch (activeTab) {
                case 0: RenderRageTab();    break;
                case 1: RenderLegitTab();   break;
                case 2: RenderVisualsTab(); break;
                case 3: RenderSkinsTab();   break;
                case 4: RenderMiscTab();    break;
                }
            }
            ImGui::EndChild();
            ImGui::PopItemWidth();
            ImGui::Unindent(16);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    ImGui::EndGroup();

    if (g_necusFont) ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar();

    // ── Bind popup (outside main window so it floats freely) ─────────────────
    RenderBindPopup();

    // ── All Binds window ──────────────────────────────────────────────────────
    if (g_showAllBinds) {
        ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.10f, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_Border, Col::Border2);
        ImGui::Begin("All Binds##panel", &g_showAllBinds,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
        if (g_necusFont) ImGui::PushFont(g_necusFont);
        ImGui::PushStyleColor(ImGuiCol_Text, Col::TextMid);
        ImGui::TextUnformatted("Feature");       ImGui::SameLine(140);
        ImGui::TextUnformatted("Key");           ImGui::SameLine(200);
        ImGui::TextUnformatted("Mode");
        ImGui::PopStyleColor();
        ImGui::Separator();
        int drawn = 0;
        for (auto& [lbl, bs] : g_binds) {
            if (bs.vks.empty()) continue;
            std::string keysStr;
            for (int ki = 0; ki < (int)bs.vks.size(); ++ki) {
                if (ki) keysStr += '+';
                keysStr += VkName(bs.vks[ki]);
            }
            ImGui::PushStyleColor(ImGuiCol_Text, Col::TextHi);
            ImGui::TextUnformatted(lbl.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(140);
            ImGui::PushStyleColor(ImGuiCol_Text, bs.active ? Col::Accent : Col::TextMid);
            ImGui::TextUnformatted(keysStr.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(200);
            const char* modeNames[] = { "Hold", "Toggle", "Always" };
            ImGui::PushStyleColor(ImGuiCol_Text, Col::TextLo);
            ImGui::TextUnformatted(modeNames[(int)bs.mode]);
            ImGui::PopStyleColor();
            ++drawn;
        }
        if (drawn == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, Col::TextLo);
            ImGui::TextUnformatted("No binds set. RMB any feature label to bind it.");
            ImGui::PopStyleColor();
        }
        if (g_necusFont) ImGui::PopFont();
        ImGui::End();
        ImGui::PopStyleColor(2);
    }
}
