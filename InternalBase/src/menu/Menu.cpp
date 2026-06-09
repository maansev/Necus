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
#include "../core/Config.h"
#include "PoppinsFont.h"   // embedded Poppins TTF (gamesense v2 reference typeface)
#include <string>
#include <vector>
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
}

// ─────────────────────────────────────────────────────────────────────────────
//  SEARCH + KEYBIND CAPTURE
// ─────────────────────────────────────────────────────────────────────────────
static char  g_SearchBuf[128] = {};
static bool  g_SearchActive = false;
static int* g_kbListening = nullptr;          // pointer to the VK being rebound
static char  g_cfgName[64] = "default";        // shared by header Save + Config tab

// Logo texture: set this once from your render backend (see LogoLoader_D3D11.h).
// If left null, a drawn orange fallback emblem is used so the menu still looks right.
static ImTextureID g_logoTex = (ImTextureID)0;
void NecusSetLogo(ImTextureID tex) { g_logoTex = tex; }

static bool MatchSearch(const char* text) {
    if (!g_SearchActive || g_SearchBuf[0] == '\0') return true;
    std::string label(text), query(g_SearchBuf);
    std::transform(label.begin(), label.end(), label.begin(), ::tolower);
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
    return label.find(query) != std::string::npos;
}

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

static void UpdateKeybindCapture() {
#ifdef _WIN32
    if (!g_kbListening) return;
    for (int vk = 1; vk < 256; ++vk) {
        if (vk == 0x01) continue;                       // ignore the LMB that opened the chip
        if (GetAsyncKeyState(vk) & 0x8000) {
            *g_kbListening = (vk == 0x1B) ? 0 : vk;      // ESC clears
            g_kbListening = nullptr;
            break;
        }
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  STYLED WIDGETS
// ─────────────────────────────────────────────────────────────────────────────
namespace UI {

    static const float kHeaderH = 30.f;
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
        ImGui::Dummy(ImVec2(g_card.w, kHeaderH + 6.f));  // reserve header space
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

        ImGui::Dummy(ImVec2(0, 9.f));
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
        ImGui::Dummy(ImVec2(0, 12.f));                   // gap between cards
    }

    static void ColTitle(const char* t) {
        ImGui::Dummy(ImVec2(0, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text, Col::TextHi);
        ImGui::TextUnformatted(t);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 6.f));
    }

    // small keybind chip; right edge anchored at rightX; rebinds *vk on click
    static void KeybindChip(const char* id, int* vk, float rightX, float cy) {
        const char* name = (g_kbListening == vk) ? "..." : VkName(*vk);
        ImVec2 ts = ImGui::CalcTextSize(name);
        float w = ImClamp(ts.x + 14.f, 34.f, 80.f), h = 18.f;
        ImGui::SetCursorScreenPos(ImVec2(rightX - w, cy - h * 0.5f));
        ImGui::PushID(id);
        ImGui::InvisibleButton("kb", ImVec2(w, h));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) g_kbListening = vk;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        dl->AddRectFilled(mn, mx, U32(ImVec4(1, 1, 1, hov ? 0.08f : 0.05f)), 4.f);
        ImU32 tc = (g_kbListening == vk) ? U32(Col::Accent) : U32(Col::TextMid);
        dl->AddText(ImVec2(mn.x + (w - ts.x) * 0.5f, mn.y + (h - ts.y) * 0.5f), tc, name);
        ImGui::PopID();
    }

    // checkbox row: label left, [keybind chip] + box right-aligned.
    // locked = greyed out and non-interactive (used for Rage/Legit mutual exclusion).
    static bool Checkbox(const char* label, bool* v, int* kb = nullptr, bool locked = false) {
        if (!MatchSearch(label)) return false;
        g_card.matches++;
        ImGui::PushID(label);
        float w0 = ImGui::GetContentRegionAvail().x - kPadX;   // leave right padding (symmetric with left)
        ImVec2 p = ImGui::GetCursorScreenPos();
        float rowH = 22.f, box = 16.f, cy = p.y + rowH * 0.5f;
        // full-row hit (label area) toggles
        ImGui::InvisibleButton("row", ImVec2(w0 - box - 8.f, rowH));
        bool changed = false;
        if (!locked && ImGui::IsItemClicked()) { *v = !*v; changed = true; }
        bool hov = !locked && ImGui::IsItemHovered();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec4 lblCol = locked ? Col::TextLo : (hov ? Col::TextHi : ImVec4(1, 1, 1, 0.78f));
        dl->AddText(ImVec2(p.x, cy - ImGui::GetTextLineHeight() * 0.5f), U32(lblCol), label);
        // keybind chip just left of the box (hidden while locked)
        float boxX = p.x + w0 - box;
        if (kb && !locked) KeybindChip(label, kb, boxX - 8.f, cy);
        // box
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
            dl->AddLine(ImVec2(b0.x + 4.0f, cy + 0.5f), ImVec2(b0.x + 6.6f, cy + 3.2f), ck, 1.8f);
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
    NecusState::RageAim& A = g_aim[g_rageWeapon];
    const char* badge = kWeapons[g_rageWeapon];
    bool lockRage = LegitActive();   // can't enable Rage while Legit is on

    float gap = 14.f, colW = (ImGui::GetContentRegionAvail().x - gap) / 2.f;

    // ── Column A: Aimbot (per-weapon) ────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("rageA", ImVec2(colW, 0), false, ImGuiWindowFlags_NoScrollbar);
    UI::ColTitle("Aimbot");

    UI::BeginCard("General", badge);
    {
        UI::Checkbox("Enabled", &A.enabled, &A.kb_enabled, lockRage);
        static const char* tgt[] = { "Players", "Lethal first", "Smart" };
        UI::MultiCombo("Target selection", tgt, A.targets, 3);
        UI::Checkbox("Automatic fire", &A.autofire);
        UI::Checkbox("Automatic scope", &A.autoscope);
        UI::Checkbox("Quick peek assist", &A.quickpeek, &A.kb_quickpeek);
        UI::SliderI("FOV", &A.fov, 0, 180, "%.0f\xC2\xB0");
    }
    UI::EndCard();

    UI::BeginCard("Accuracy", badge);
    {
        UI::SliderI("Minimum hit chance", &A.hitchance, 0, 100, "%.0f%%");
        UI::SliderI("Minimum damage", &A.mindmg, 0, 100, "%.0f");
        UI::Checkbox("Damage override", &A.dmgoverride, &A.kb_dmgoverride);
        static const char* hb[] = { "Head", "Chest", "Stomach", "Arms", "Legs", "Feet" };
        UI::MultiCombo("Hitboxes", hb, A.hitbox, 6);
        static const char* mp[] = { "Off", "Low", "Medium", "High" };
        UI::Combo("Multipoint", &A.multipoint, mp, 4);
        UI::SliderI("Min. multipoint", &A.minmp, 0, 100, "%.0f%%");
    }
    UI::EndCard();
    ImGui::EndChild();

    ImGui::SameLine(0, gap);

    // ── Column B: Anti-Aim (global) ──────────────────────────────────────────
    ImGui::BeginChild("rageB", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
    UI::ColTitle("Anti-Aim");

    UI::BeginCard("Exploits");
    {
        UI::Checkbox("Double tap", &g_aa.doubletap, &g_aa.kb_doubletap);
        UI::SliderI("DT tolerance", &g_aa.dttolerance, 0, 21, "%.0f");
        UI::Checkbox("On shot anti-aim", &g_aa.hideshots, &g_aa.kb_hideshots);
    }
    UI::EndCard();

    UI::BeginCard("Angles");
    {
        UI::Checkbox("Enabled", &g_aa.aa_enabled);
        static const char* pitch[] = { "Off", "Default", "Up", "Down", "Minimal" };
        UI::Combo("Pitch", &g_aa.pitch, pitch, 5);
        static const char* yb[] = { "Local view", "At targets" };
        UI::Combo("Yaw base", &g_aa.yawbase, yb, 2);
        static const char* yaw[] = { "Off", "180", "Spin", "Static" };
        UI::Combo("Yaw", &g_aa.yaw, yaw, 4);
        static const char* jit[] = { "Off", "Offset", "Center", "Random" };
        UI::Combo("Jitter", &g_aa.jitter, jit, 4);
        static const char* by[] = { "Off", "Static", "Jitter", "Opposite" };
        UI::Combo("Body yaw", &g_aa.bodyyaw, by, 4);
        UI::Checkbox("Freestanding", &g_aa.freestanding, &g_aa.kb_free);
    }
    UI::EndCard();

    UI::BeginCard("Fake Lag");
    {
        UI::Checkbox("Enabled", &g_aa.fakelag);
        static const char* fl[] = { "Static", "Adaptive", "Maximum" };
        UI::Combo("Mode", &g_aa.flmode, fl, 3);
        UI::SliderI("Limit", &g_aa.fllimit, 0, 16, "%.0f");
    }
    UI::EndCard();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderLegitTab() {
    using namespace NecusState;
    // ── Per-weapon swap ──────────────────────────────────────────────────────
    // Keep the selected weapon's legit config live in Globals (backend reads it);
    // save/restore the rest in g_legitSlots. One-time init seeds every slot with
    // your current Globals defaults.
    static bool initd = false;
    static int  prevW = 0;
    if (!initd) {
        for (int i = 0; i < kWeaponCount; ++i) {
            g_legitSlots[i].enabled = Globals::aimbot_enabled;
            g_legitSlots[i].hitbox = Globals::aimbot_target_hitbox;
            g_legitSlots[i].fov = Globals::aimbot_fov;
            g_legitSlots[i].smooth = Globals::aimbot_smooth;
            g_legitSlots[i].vis = Globals::aimbot_visibility_check;
            g_legitSlots[i].ff = Globals::aimbot_friendly_fire;
        }
        prevW = g_legitWeapon; initd = true;
    }
    if (g_legitWeapon != prevW) {
        // save outgoing weapon
        g_legitSlots[prevW].enabled = Globals::aimbot_enabled;
        g_legitSlots[prevW].hitbox = Globals::aimbot_target_hitbox;
        g_legitSlots[prevW].fov = Globals::aimbot_fov;
        g_legitSlots[prevW].smooth = Globals::aimbot_smooth;
        g_legitSlots[prevW].vis = Globals::aimbot_visibility_check;
        g_legitSlots[prevW].ff = Globals::aimbot_friendly_fire;
        // load incoming weapon
        Globals::aimbot_enabled = g_legitSlots[g_legitWeapon].enabled;
        Globals::aimbot_target_hitbox = g_legitSlots[g_legitWeapon].hitbox;
        Globals::aimbot_fov = g_legitSlots[g_legitWeapon].fov;
        Globals::aimbot_smooth = g_legitSlots[g_legitWeapon].smooth;
        Globals::aimbot_visibility_check = g_legitSlots[g_legitWeapon].vis;
        Globals::aimbot_friendly_fire = g_legitSlots[g_legitWeapon].ff;
        prevW = g_legitWeapon;
    }
    const char* badge = kWeapons[g_legitWeapon];
    bool lockLegit = RageActive();    // can't enable Legit while Rage is on

    float gap = 14.f, colW = (ImGui::GetContentRegionAvail().x - gap) / 2.f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

    ImGui::BeginChild("legitA", ImVec2(colW, 0), false, ImGuiWindowFlags_NoScrollbar);
    UI::ColTitle("Aimbot");
    UI::BeginCard("Aimbot", badge);
    {
        UI::Checkbox("Enable Aimbot", &Globals::aimbot_enabled, nullptr, lockLegit);
        if (Globals::aimbot_enabled) {
            static const char* hb[] = { "Head", "Neck", "Chest", "Pelvis", "Closest Bone" };
            UI::Combo("Target Bone", &Globals::aimbot_target_hitbox, hb, IM_ARRAYSIZE(hb));
            UI::SliderF("FOV", &Globals::aimbot_fov, 0.f, 45.f, "%.1f");
            UI::SliderF("Smoothing", &Globals::aimbot_smooth, 0.f, 10.f, "%.1f");
            UI::Checkbox("Visibility Check", &Globals::aimbot_visibility_check);
            UI::Checkbox("Friendly Fire", &Globals::aimbot_friendly_fire);
        }
    }
    UI::EndCard();
    ImGui::EndChild();

    ImGui::SameLine(0, gap);

    ImGui::BeginChild("legitB", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
    UI::ColTitle("Combat");
    UI::BeginCard("RCS & Triggerbot");
    {
        UI::Checkbox("Enable RCS", &Globals::rcs_enabled);
        if (Globals::rcs_enabled) {
            UI::SliderF("Pitch Scale", &Globals::rcs_x_scale, 0.f, 2.f, "%.2f");
            UI::SliderF("Yaw Scale", &Globals::rcs_y_scale, 0.f, 2.f, "%.2f");
        }
        UI::Checkbox("Enable Triggerbot", &Globals::triggerbot_enabled);
        if (Globals::triggerbot_enabled) {
            UI::SliderI("Delay (ms)", &Globals::triggerbot_delay, 0, 500, "%.0f");
            static const char* tb[] = { "Any Hitbox", "Head Only" };
            UI::Combo("Hitbox Filter", &Globals::triggerbot_hitbox_filter, tb, IM_ARRAYSIZE(tb));
        }
    }
    UI::EndCard();
    UI::BeginCard("Backtrack");
    {
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
        ImGui::BeginChild("visA", ImVec2(colW, 0), false, ImGuiWindowFlags_NoScrollbar);
        UI::ColTitle(e ? "Enemies" : "Teammates");
        UI::BeginCard(e ? "Enemy ESP" : "Teammate ESP");
        {
            UI::Checkbox("Enable ESP", e ? &Globals::esp_enabled : &Globals::esp_teammates);
            UI::Checkbox("Box", e ? &Globals::enemy_box : &Globals::team_box);
            if (*(e ? &Globals::enemy_box : &Globals::team_box)) {
                static const char* st[] = { "2D", "Corners" };
                UI::Combo("Style", e ? &Globals::enemy_box_style : &Globals::team_box_style, st, IM_ARRAYSIZE(st));
                UI::Checkbox("Fill", e ? &Globals::enemy_box_fill : &Globals::team_box_fill);
                if (*(e ? &Globals::enemy_box_fill : &Globals::team_box_fill))
                    UI::Color("Fill Color", e ? Globals::enemy_box_fill_color : Globals::team_box_fill_color);
                UI::Color("Box Color", e ? Globals::esp_box_color : Globals::esp_teammate_color);
                UI::SliderF("Thickness", &Globals::esp_box_thickness, 1.f, 4.f, "%.1f");
            }
            UI::Checkbox("Skeleton", e ? &Globals::enemy_skeleton : &Globals::team_skeleton);
            if (*(e ? &Globals::enemy_skeleton : &Globals::team_skeleton)) {
                UI::Color("Bone Color", e ? Globals::esp_skeleton_color : Globals::team_skeleton_color);
                UI::SliderF("Bone Thickness", &Globals::esp_skeleton_thickness, 1.f, 4.f, "%.1f");
            }
            UI::Checkbox("Name", e ? &Globals::enemy_name : &Globals::team_name);
            if (*(e ? &Globals::enemy_name : &Globals::team_name)) {
                UI::Color("Name Color", e ? Globals::esp_name_color : Globals::team_name_color);
                static const char* pos[] = { "Top", "Bottom" };
                UI::Combo("Name Pos", e ? &Globals::enemy_name_position : &Globals::team_name_position, pos, IM_ARRAYSIZE(pos));
            }
            UI::Checkbox("Health", e ? &Globals::enemy_health : &Globals::team_health);
            if (*(e ? &Globals::enemy_health : &Globals::team_health)) {
                static const char* hp[] = { "Top", "Bottom", "Left", "Right" };
                UI::Combo("HP Pos", e ? &Globals::enemy_health_position : &Globals::team_health_position, hp, IM_ARRAYSIZE(hp));
                UI::Checkbox("HP Text", e ? &Globals::enemy_health_text : &Globals::team_health_text);
            }
            UI::Checkbox("Armor", e ? &Globals::enemy_armor : &Globals::team_armor);
            UI::Checkbox("Weapon", e ? &Globals::enemy_weapon : &Globals::team_weapon);
            if (*(e ? &Globals::enemy_weapon : &Globals::team_weapon)) {
                static const char* wp[] = { "Top", "Bottom" };
                UI::Combo("Weapon Pos", e ? &Globals::enemy_weapon_position : &Globals::team_weapon_position, wp, IM_ARRAYSIZE(wp));
                static const char* wm[] = { "Text", "Icons" };
                UI::Combo("Weapon Style", e ? &Globals::enemy_weapon_display_mode : &Globals::team_weapon_display_mode, wm, IM_ARRAYSIZE(wm));
                UI::Checkbox("Ammo", e ? &Globals::enemy_ammo : &Globals::team_ammo);
            }
            UI::Checkbox("Distance", e ? &Globals::enemy_distance : &Globals::team_distance);
            UI::Checkbox("Flags", e ? &Globals::enemy_flags : &Globals::team_flags);
        }
        UI::EndCard();
        ImGui::EndChild();

        ImGui::SameLine(0, gap);

        ImGui::BeginChild("visB", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
        UI::ColTitle("Materials");
        UI::BeginCard("Chams");
        {
            UI::Checkbox("Enable Chams", e ? &Globals::enemy_chams_enabled : &Globals::team_chams_enabled);
            if (*(e ? &Globals::enemy_chams_enabled : &Globals::team_chams_enabled)) {
                UI::Color("Visible Color", e ? Globals::enemy_chams_visible_color : Globals::team_chams_visible_color);
                UI::Color("XQZ Color", e ? Globals::enemy_chams_invisible_color : Globals::team_chams_invisible_color);
                static const char* m[] = { "Flat", "Illuminate", "Glow" };
                UI::Combo("Material", e ? &Globals::enemy_chams_material : &Globals::team_chams_material, m, IM_ARRAYSIZE(m));
            }
        }
        UI::EndCard();
        UI::BeginCard("Glow");
        {
            UI::Checkbox("Enable Glow", e ? &Globals::enemy_glow_enabled : &Globals::team_glow_enabled);
            if (*(e ? &Globals::enemy_glow_enabled : &Globals::team_glow_enabled)) {
                UI::Color("Glow Color", e ? Globals::enemy_glow_color : Globals::team_glow_color);
                UI::SliderF("Brightness", e ? &Globals::enemy_glow_brightness : &Globals::team_glow_brightness, 0.1f, 3.0f, "%.2f");
                static const char* g[] = { "Normal", "Saturated", "Outline", "Thick" };
                UI::Combo("Glow Style", e ? &Globals::enemy_glow_style : &Globals::team_glow_style, g, IM_ARRAYSIZE(g));
                UI::Checkbox("Skeleton Glow", e ? &Globals::enemy_glow_skeleton : &Globals::team_glow_skeleton);
                if (*(e ? &Globals::enemy_glow_skeleton : &Globals::team_glow_skeleton)) {
                    UI::Color("Skel Glow Color", e ? Globals::enemy_glow_skel_color : Globals::team_glow_skel_color);
                    UI::SliderF("Skel Thickness", e ? &Globals::enemy_glow_skel_thickness : &Globals::team_glow_skel_thickness, 1.f, 5.f, "%.1f");
                }
            }
        }
        UI::EndCard();
        ImGui::EndChild();
    }
    else { // World
        ImGui::BeginChild("worldA", ImVec2(colW, 0), false, ImGuiWindowFlags_NoScrollbar);
        UI::ColTitle("World");
        UI::BeginCard("World ESP");
        {
            UI::Checkbox("Dropped Weapons", &Globals::world_dropped_weapons);
        }
        UI::EndCard();
        ImGui::EndChild();

        ImGui::SameLine(0, gap);

        ImGui::BeginChild("worldB", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
        UI::ColTitle("Environment");
        UI::BeginCard("Night Mode");
        {
            UI::Checkbox("Enable Night Mode", &Globals::misc_nightmode);
            if (Globals::misc_nightmode)
                UI::SliderF("Brightness", &Globals::misc_nightmode_brightness, 0.05f, 1.0f, "%.2f");
            UI::Checkbox("No Flash", &Globals::misc_noflash);
            UI::Checkbox("No Smoke", &Globals::misc_nosmoke);
        }
        UI::EndCard();
        ImGui::EndChild();
    }
    ImGui::PopStyleColor();
}

static void RenderSkinsTab() {
    using namespace NecusState;
    float gap = 14.f, colW = (ImGui::GetContentRegionAvail().x - gap) / 2.f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

    ImGui::BeginChild("skinA", ImVec2(colW, 0), false, ImGuiWindowFlags_NoScrollbar);
    UI::ColTitle("Weapons");
    UI::BeginCard("Knife");
    {
        UI::Checkbox("Enabled", &g_skins.knife_on);
        static const char* km[] = { "Default", "Karambit", "M9 Bayonet", "Butterfly", "Talon", "Skeleton" };
        UI::Combo("Model", &g_skins.knife_model, km, IM_ARRAYSIZE(km));
        static const char* kp[] = { "Fade", "Doppler", "Marble Fade", "Tiger Tooth", "Lore", "Autotronic" };
        UI::Combo("Paint kit", &g_skins.knife_paint, kp, IM_ARRAYSIZE(kp));
    }
    UI::EndCard();
    UI::BeginCard("Gloves");
    {
        UI::Checkbox("Enabled", &g_skins.gloves_on);
        static const char* gm[] = { "Default", "Sport", "Driver", "Specialist", "Hydra" };
        UI::Combo("Model", &g_skins.glove_model, gm, IM_ARRAYSIZE(gm));
        static const char* gp[] = { "Pandora", "Vice", "Superconductor", "Amphibious", "Omega" };
        UI::Combo("Paint kit", &g_skins.glove_paint, gp, IM_ARRAYSIZE(gp));
    }
    UI::EndCard();
    ImGui::EndChild();

    ImGui::SameLine(0, gap);

    ImGui::BeginChild("skinB", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
    UI::ColTitle("Settings");
    UI::BeginCard("Apply");
    {
        static const char* aw[] = { "AK-47", "M4A4", "M4A1-S", "AWP", "USP-S", "Glock-18", "Desert Eagle" };
        UI::Combo("Active weapon", &g_skins.active, aw, IM_ARRAYSIZE(aw));
        static const char* pk[] = { "Asiimov", "Redline", "Vulcan", "Hyper Beast", "Neon Rider", "Fire Serpent" };
        UI::Combo("Paint kit", &g_skins.paint, pk, IM_ARRAYSIZE(pk));
        UI::SliderI("Wear", &g_skins.wear, 0, 100, "%.0f%%");
        UI::SliderI("Pattern seed", &g_skins.seed, 0, 1000, "%.0f");
        ImGui::Dummy(ImVec2(0, 2));
        if (UI::StyledButton("Apply skin changes", ImVec2(-UI::kPadX, 30), true)) { /* hook backend */ }
    }
    UI::EndCard();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderMiscTab() {
    float gap = 14.f, colW = (ImGui::GetContentRegionAvail().x - gap) / 2.f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

    // ── Left column: Movement, then Settings underneath ──────────────────────
    ImGui::BeginChild("miscA", ImVec2(colW, 0), false, ImGuiWindowFlags_NoScrollbar);
    UI::ColTitle("Movement");
    UI::BeginCard("Movement");
    {
        UI::Checkbox("BunnyHop", &Globals::misc_bhop);
        UI::Checkbox("Auto Strafer", &Globals::misc_autostrafer);
        UI::Checkbox("Infinite Duck", &Globals::misc_infinite_duck);
    }
    UI::EndCard();

    UI::ColTitle("Settings");
    UI::BeginCard("Visuals & Helpers");
    {
        UI::Checkbox("No Flash", &Globals::misc_noflash);
        if (Globals::misc_noflash)
            UI::SliderF("Flash Alpha", &Globals::misc_flash_alpha, 0.f, 255.f, "%.0f");
        UI::Checkbox("No Smoke", &Globals::misc_nosmoke);
        UI::Checkbox("Sniper Crosshair", &Globals::misc_sniper_crosshair);
        UI::Checkbox("Reveal Ranks", &Globals::misc_reveal_ranks);
        UI::Checkbox("Spectator List", &Globals::misc_spectator_list);
        UI::Checkbox("2D Radar", &Globals::misc_radar_2d);
        UI::Checkbox("Hitmarker", &Globals::misc_hitmarker);
        if (Globals::misc_hitmarker) {
            static const char* s[] = { "None", "CoD", "Bell" };
            UI::Combo("Sound", &Globals::misc_hitmarker_sound, s, IM_ARRAYSIZE(s));
        }
        UI::Checkbox("Damage Logs", &Globals::misc_damage_logs);
        UI::Checkbox("Watermark", &Globals::misc_watermark);
    }
    UI::EndCard();
    ImGui::EndChild();

    ImGui::SameLine(0, gap);

    // ── Right column: Configuration ──────────────────────────────────────────
    ImGui::BeginChild("miscB", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
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
        if (UI::StyledButton("Delete", ImVec2(bw, 30))) { Config::DeleteConfig(g_cfgName); memset(g_cfgName, 0, sizeof(g_cfgName)); }

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
    ImGui::EndChild();
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
ImFont* g_necusFont = nullptr;

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

            // Save button (icon + label)
            ImGui::SetCursorPos(ImVec2(16, 11));
            ImGui::InvisibleButton("savebtn", ImVec2(84, 32));
            {
                bool hov = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked()) Config::Save(g_cfgName);
                ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
                float cy = (mn.y + mx.y) * 0.5f;
                hdl->AddRectFilled(mn, mx, U32(hov ? Col::FrameHi : Col::Frame), 7.f);
                hdl->AddRect(mn, mx, U32(hov ? Col::Accent : Col::Border), 7.f, 0, 1.f);
                DrawSaveIcon(hdl, ImVec2(mn.x + 17, cy), U32(hov ? Col::Accent : Col::TextMid));
                hdl->AddText(ImVec2(mn.x + 31, cy - ImGui::GetTextLineHeight() * 0.5f), U32(Col::TextHi), "Save");
            }

            // Weapon picker — only on Rage & Legit (both are per-weapon aimbots)
            bool showPicker = (activeTab == 0 || activeTab == 1);
            int* wIdx = (activeTab == 1) ? &NecusState::g_legitWeapon : &NecusState::g_rageWeapon;
            ImVec2 wMn(0, 0), wMx(0, 0);
            if (showPicker) {
                ImGui::SameLine(0, 10);
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

            // Search (right-aligned)
            float searchW = 200.f;
            ImGui::SameLine();
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
            switch (activeTab) {
            case 0: RenderRageTab();    break;
            case 1: RenderLegitTab();   break;
            case 2: RenderVisualsTab(); break;
            case 3: RenderSkinsTab();   break;
            case 4: RenderMiscTab();    break;
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
}
