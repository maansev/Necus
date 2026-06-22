#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "Aimbot.h"
#include "../../../sdk/entity/EntityManager.h"
#include "../../../sdk/memory/Offsets.h"
#include "../../../sdk/memory/PatternScan.h"
#include "../../../sdk/utils/Globals.h"
#include "../../../sdk/utils/Utils.h"
#include "../../../sdk/utils/Log.h"
#include "../../../menu/Menu.h"
#include "../../Rage/Rage.h"
#include <Windows.h>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <deque>

using clk = std::chrono::steady_clock;

// ─── Internal offsets ────────────────────────────────────────────────────────
static constexpr ptrdiff_t AB_VIEW_ANGLES = 0x23568C8; // cs2-dumper 2026-06-10

// ─── Hitbox → BoneID ─────────────────────────────────────────────────────────
// Slots: 0=Head 1=Neck 2=Chest 3=Stomach 4=Arms 5=Legs
static const BoneID kBoneMap[6] = {
    BoneID::Head,
    BoneID::Neck,
    BoneID::Chest,
    BoneID::Pelvis,
    BoneID::LeftShoulder,
    BoneID::LeftHip,
};

// ─── Aimbot state ────────────────────────────────────────────────────────────
static C_CSPlayerPawn* s_target      = nullptr;
static clk::time_point s_targetSince    = clk::now();
static clk::time_point s_lastKillTime   = clk::now();
static bool            s_hadTarget      = false;

// Smooth: persistent interpolated angle
static Vector          s_smoothAngle  = {};
static bool            s_smoothValid  = false;
static clk::time_point s_lastFrame    = clk::now();

// ─── Triggerbot state ─────────────────────────────────────────────────────────
static bool            s_trigActive   = false;
static clk::time_point s_trigStart    = clk::now();

// ─── Backtrack ───────────────────────────────────────────────────────────────
static constexpr int kBTSlots = 16;

struct BoneSnap {
    Vector               bones[6];
    clk::time_point      t;
};

struct BTHistory {
    BoneSnap records[kBTSlots];
    int      head  = 0;
    int      count = 0;
};

static std::unordered_map<uintptr_t, BTHistory> s_btHist;

// ─── Helpers ─────────────────────────────────────────────────────────────────
static inline float* ViewAnglesPtr(uintptr_t client) {
    return reinterpret_cast<float*>(client + AB_VIEW_ANGLES);
}
static inline Vector ReadViewAngles(uintptr_t client) {
    float* p = ViewAnglesPtr(client);
    return { p[0], p[1], 0.f };
}
static inline void WriteViewAngles(uintptr_t client, const Vector& a) {
    float* p = ViewAnglesPtr(client);
    p[0] = a.x; p[1] = a.y; p[2] = 0.f;
}
static inline void WriteEyeAngles(C_CSPlayerPawn* pawn, const Vector& a) {
    float* p = reinterpret_cast<float*>((uintptr_t)pawn + Offsets::m_angEyeAngles);
    p[0] = a.x; p[1] = a.y; p[2] = 0.f;
}
static inline float AngleFov(const Vector& a, const Vector& b) {
    Vector d = { a.x - b.x, a.y - b.y, 0.f };
    Utils::NormalizeAngles(d);
    return sqrtf(d.x * d.x + d.y * d.y);
}

// ─── Record backtrack history every frame ────────────────────────────────────
static void RecordBacktrack() {
    if (!Globals::backtrack_enabled) return;
    auto now = clk::now();
    for (const auto& e : EntityManager::Get().GetEntities()) {
        if (!e.isEnemy || !e.pawn || !e.pawn->IsAlive()) continue;
        auto& hist = s_btHist[(uintptr_t)e.pawn];
        auto& rec  = hist.records[hist.head];
        rec.t = now;
        for (int i = 0; i < 6; ++i)
            rec.bones[i] = Utils::GetBonePos(e.pawn, kBoneMap[i]);
        hist.head  = (hist.head + 1) % kBTSlots;
        hist.count = std::min(hist.count + 1, kBTSlots);
    }
}

// Find best historical position within time window for given entity/bone
static bool BestBacktrackBone(C_CSPlayerPawn* pawn, int boneIdx,
                               const Vector& eye, const Vector& view,
                               Vector& outPos, float& outFov)
{
    auto it = s_btHist.find((uintptr_t)pawn);
    if (it == s_btHist.end()) return false;
    auto& hist = it->second;
    auto  now  = clk::now();
    float btMs = Globals::backtrack_time;

    bool  found   = false;
    float bestFov = 1e9f;
    for (int i = 0; i < hist.count; ++i) {
        int idx = (hist.head - 1 - i + kBTSlots) % kBTSlots;
        auto& rec = hist.records[idx];
        float ms = std::chrono::duration_cast<std::chrono::microseconds>(
                       now - rec.t).count() / 1000.f;
        if (ms > btMs) break;
        const Vector& bp = rec.bones[boneIdx];
        if (bp.IsZero()) continue;
        Vector ang = Utils::CalcAngle(eye, bp);
        Utils::NormalizeAngles(ang);
        float fov = AngleFov(ang, view);
        if (fov < bestFov) { bestFov = fov; outPos = bp; outFov = fov; found = true; }
    }
    return found;
}

// Scan all selected hitboxes. If backtrack enabled, also check history.
static bool BestBoneForTarget(C_CSPlayerPawn* pawn, const Vector& eye,
                               const Vector& view, float fovLimit,
                               Vector& outPos, float& outFov)
{
    float best  = fovLimit;
    bool  found = false;
    for (int i = 0; i < 6; ++i) {
        if (!Globals::aimbot_hitboxes[i]) continue;

        // Current position
        Vector bp = Utils::GetBonePos(pawn, kBoneMap[i]);
        if (!bp.IsZero()) {
            Vector ang = Utils::CalcAngle(eye, bp);
            Utils::NormalizeAngles(ang);
            float fov = AngleFov(ang, view);
            if (fov < best) { best = fov; outPos = bp; outFov = fov; found = true; }
        }

        // Backtrack history
        if (Globals::backtrack_enabled) {
            Vector bhp; float bfov;
            if (BestBacktrackBone(pawn, i, eye, view, bhp, bfov) && bfov < best) {
                best = bfov; outPos = bhp; outFov = bfov; found = true;
            }
        }
    }
    return found;
}

// ─── RCS ─────────────────────────────────────────────────────────────────────
// ─── Recoil Control System ───────────────────────────────────────────────────
// Internal legit RCS: reads m_aimPunchAngle (the server-driven recoil vector),
// tracks its delta frame-to-frame, and subtracts that delta from the local view
// angles so the crosshair stays on target through a full spray.
//
// WHY delta, not total:
//   m_aimPunchAngle grows upward during the spray. The game adds it to the
//   rendered/fired angle on top of the camera angle, so the screen drifts UP.
//   To counter: each frame subtract however much the punch GREW since last frame.
//   This keeps the camera moving in the opposite direction at the same rate.
//   Net recoil on screen → 0.  (Matching scale=1.0 is full compensation.)
//
// WHY always update s_prevPunch:
//   If we only update it while firing, the gap between bursts accumulates
//   a stale delta. On the next burst the first compensation is huge and wrong.
//   Updating every frame (even not-firing) keeps the delta exactly 0 when the
//   punch decays back to 0 between bursts — no jump.
//
// The ×2 multiplier: CS2 (like CS:GO) stores half the visual recoil in
//   m_aimPunchAngle; the engine doubles it for display/bullet direction.
//   So the compensated delta must also be ×2 to cancel the full visual movement.
//
// Stage: runs at FSN stage 5, AFTER client prediction has written fresh punch values.
namespace RCS {
    static Vector s_prevPunch = {};  // raw punch from the previous stage-5 call
    static int    s_prevShots = 0;

    // Called from FSN stage 0: export shot counter so CreateMove has it in time.
    static void UpdateShots(C_CSPlayerPawn* local) {
        if (!Globals::rcs_enabled) { Aimbot::g_rcsEnabled = false; return; }
        if (Globals::rcs_key != 0 && !(GetAsyncKeyState(Globals::rcs_key) & 0x8000)) {
            Aimbot::g_rcsEnabled = false; return;
        }
        int shots = 0;
        __try { shots = local->m_iShotsFired(); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        Aimbot::g_rcsShotsFired = shots;
        Aimbot::g_rcsEnabled    = true;
    }

    // Called from FSN stage 5: punch is valid after prediction, apply compensation.
    static void RunAtStage5(C_CSPlayerPawn* local, uintptr_t client) {
        // Always reset export state so the overlay stays accurate.
        Aimbot::g_rcsPunchPitch = 0.f;
        Aimbot::g_rcsPunchYaw   = 0.f;

        if (!Globals::rcs_enabled) { s_prevPunch = {}; s_prevShots = 0; return; }
        if (Globals::rcs_key != 0 && !(GetAsyncKeyState(Globals::rcs_key) & 0x8000)) {
            s_prevPunch = {}; s_prevShots = 0; return;
        }

        int    shots = 0;
        Vector punch = {};
        __try {
            shots = local->m_iShotsFired();
            punch = local->m_aimPunchAngle();
        } __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        // Export for debug overlay (raw punch, no scale).
        Aimbot::g_rcsPunchPitch = punch.x;
        Aimbot::g_rcsPunchYaw   = punch.y;

        if (Globals::chams_debug && (shots % 5 == 0)) {
            Log::Write("RCS s5: shots=%d punch=(%.3f,%.3f) prev=(%.3f,%.3f)",
                       shots, punch.x, punch.y, s_prevPunch.x, s_prevPunch.y);
        }

        // Delta since last frame (always computed, even when not firing).
        Vector delta = { punch.x - s_prevPunch.x, punch.y - s_prevPunch.y, 0.f };

        // CRITICAL: always update prevPunch before any early-return.
        // If we skip this between bursts, the next burst's first delta is huge.
        s_prevPunch = punch;
        s_prevShots = shots;

        // Gate: only apply while actively shooting (skip the 1st shot — it has
        // no recoil yet and compensation would push the aim down needlessly).
        bool firing = (shots > Globals::rcs_delay_shots);
        if (!firing) return;

        // Skip trivially small deltas (prevents micro-jitter when idle).
        if (std::abs(delta.x) < 0.0001f && std::abs(delta.y) < 0.0001f) return;

        // Scale: user sets 0..1 where 1 = 100% compensation.
        // The ×2 multiplier matches the engine's visual doubling of m_aimPunchAngle.
        float cx = delta.x * 2.f * Globals::rcs_x_scale;
        float cy = delta.y * 2.f * Globals::rcs_y_scale;

        // Safety clamp: prevents a single huge delta (corrupt read) from
        // flinging the camera off-screen. 5° per frame is already extreme.
        cx = std::clamp(cx, -Globals::rcs_max_x, Globals::rcs_max_x);
        cy = std::clamp(cy, -Globals::rcs_max_y, Globals::rcs_max_y);

        Vector view = ReadViewAngles(client);
        view.x -= cx;
        view.y -= cy;
        Utils::NormalizeAngles(view);
        WriteViewAngles(client, view);
        WriteEyeAngles(local, view);
    }
} // namespace RCS

// ─── Weapon exclusion ────────────────────────────────────────────────────────
// Reads active weapon def_index via WeaponServices. Returns 0 on failure.
// MUST be called from a function WITHOUT __try if a vector lives in the same scope
// (no vectors here, so __try is fine).
static uint16_t GetActiveWeaponDef(C_CSPlayerPawn* pawn)
{
    uint16_t def = 0;
    __try {
        uintptr_t ws = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(pawn) + Offsets::m_pWeaponServices);
        if (!ws) return 0;
        uint32_t handle = *reinterpret_cast<uint32_t*>(ws + Offsets::m_hActiveWeapon);
        if (!handle || handle == 0xFFFFFFFFu) return 0;
        uintptr_t weap = EntityManager::Get().GetEntityFromHandle(handle);
        if (!weap) return 0;
        def = *reinterpret_cast<uint16_t*>(
            weap + Offsets::m_AttributeManager + 0x50 + Offsets::m_iItemDefinitionIndex);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return def;
}

// Returns true if the weapon should suppress aimbot/triggerbot.
// Knives: 42 (CT default), 59 (T default), 500-530 (all named knives).
// Zeus/Taser: 31.
// Grenades: 43-49 (flash/HE/smoke/molotov/decoy/incendiary/C4), 68-69 (breach/bump).
static bool IsExcludedWeapon(uint16_t def)
{
    if (def == 31) return true;                          // Zeus
    if (def == 42 || def == 59) return true;             // default knives
    if (def >= 43 && def <= 49) return true;             // grenades + C4
    if (def == 68 || def == 69) return true;             // breach / bump
    if (def >= 500 && def <= 530) return true;           // all CS2 named knives
    return false;
}

// Semi-auto pistols (one bullet per click). Used by Auto Pistol so holding LMB
// re-triggers them like a full-auto gun.
//  1 Deagle, 2 Dual Berettas, 3 Five-SeveN, 4 Glock, 30 Tec-9,
//  32 P2000, 36 P250, 61 USP-S, 63 CZ75-Auto, 64 R8 Revolver
static bool IsSemiPistol(uint16_t def)
{
    switch (def) {
        case 1: case 2: case 3: case 4: case 30:
        case 32: case 36: case 61: case 63: case 64:
            return true;
        default: return false;
    }
}

// True when any UI is up: the cheat menu, or a game panel (buy menu, scoreboard,
// chat, console) — all of which make the OS cursor visible. While a menu is open
// the user's LMB belongs to that UI, so auto-fire helpers must stay silent or
// they spam clicks into the menu (the buy-menu "many clicks" bug).
static bool IsAnyMenuOpen()
{
    if (Menu::IsOpen) return true;
    CURSORINFO ci = { sizeof(CURSORINFO) };
    if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING)) return true;
    return false;
}

// ─── Auto Pistol (Misc helper) ────────────────────────────────────────────────
// While the user physically holds LMB on a semi-auto pistol, re-trigger the
// click so it fires continuously like a rifle. The game caps each pistol's fire
// rate, so spamming faster than the cap is simply ignored.
static void RunAutoPistol(C_CSPlayerPawn* local)
{
    if (!Globals::misc_auto_pistol) return;
    if (IsAnyMenuOpen()) return;                            // no click-spam in menus
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;  // user must hold fire

    uint16_t def = GetActiveWeaponDef(local);
    if (!IsSemiPistol(def)) return;

    static clk::time_point s_last = clk::now();
    auto now = clk::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last).count();
    if (ms < 60) return;   // ~16 re-triggers/sec; each gun's own cap limits actual ROF
    s_last = now;

    // Release+press edge re-fires the semi-auto while the physical button is held.
    INPUT inp[2] = {};
    inp[0].type = INPUT_MOUSE; inp[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    inp[1].type = INPUT_MOUSE; inp[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(2, inp, sizeof(INPUT));
}

// ─── Per-weapon profile resolution ───────────────────────────────────────────
// Maps CS2 weapon def_index → slot index in Globals::g_legitProfiles[].
// Slot 0 = Global fallback. Indices 1-34 match NecusState::kWeapons in Menu.cpp.
static int DefToProfileSlot(uint16_t def)
{
    static const struct { uint16_t d; int s; } kMap[] = {
        { 1,  1}, {64,  2}, { 2,  3}, { 3,  4}, { 4,  5},
        {32,  6}, {61,  7}, {36,  8}, {63,  9}, {30, 10},
        {17, 11}, {34, 12}, {33, 13}, {23, 14}, {24, 15},
        {19, 16}, {26, 17}, {13, 18}, {10, 19}, { 7, 20},
        {16, 21}, {60, 22}, {39, 23}, { 8, 24}, {40, 25},
        { 9, 26}, {11, 27}, {38, 28}, {35, 29}, {25, 30},
        {27, 31}, {29, 32}, {14, 33}, {28, 34}
    };
    for (auto& m : kMap) if (m.d == def) return m.s;
    return 0; // Global
}

// Applies the active weapon's per-weapon profile to the flat Globals:: vars so
// that Visuals / Hooks / Config remain unchanged.
static void ApplyActiveProfile(C_CSPlayerPawn* local)
{
    uint16_t def  = GetActiveWeaponDef(local);
    int      slot = DefToProfileSlot(def);
    // Prefer per-weapon slot if aimbot or rcs is explicitly enabled there;
    // otherwise fall back to Global slot (0).
    const auto& pw  = Globals::g_legitProfiles[slot];
    const auto& glb = Globals::g_legitProfiles[0];
    const auto& p   = (slot != 0 && (pw.aimbot_enabled || pw.rcs_enabled)) ? pw : glb;

    Globals::aimbot_enabled  = p.aimbot_enabled;
    for (int i = 0; i < 6; ++i) Globals::aimbot_hitboxes[i] = p.hitboxes[i];
    Globals::aimbot_fov       = p.fov;
    Globals::aimbot_smooth    = p.smooth;
    Globals::aimbot_shot_delay  = p.shot_delay;
    Globals::aimbot_kill_delay  = p.kill_delay;
    Globals::aimbot_min_damage  = p.min_damage;
    Globals::aimbot_hit_chance  = p.hit_chance;
    Globals::aimbot_lock_target = p.lock_target;
    Globals::aimbot_lock_mouse  = p.lock_mouse;

    Globals::rcs_enabled       = p.rcs_enabled;
    Globals::rcs_key           = p.rcs_key;
    Globals::rcs_x_scale       = p.rcs_x_scale;
    Globals::rcs_y_scale       = p.rcs_y_scale;
    Globals::rcs_max_x         = p.rcs_max_x;
    Globals::rcs_max_y         = p.rcs_max_y;
    Globals::rcs_delay_shots   = p.rcs_delay_shots;
}

// ─── Visibility (spotted) check ───────────────────────────────────────────────
// pawn + m_entitySpottedState points at the embedded EntitySpottedState_t.
// In the CS2 schema its layout is: +0x8 bool m_bSpotted, +0xC m_bSpottedByMask[2].
// m_bSpotted is set when the entity is currently spotted by the opposing team —
// a good proxy for "visible" on enemies. Fails OPEN (returns true) on AV so a
// stale offset never silently disables the aimbot.
//
// NOTE: only consulted when Auto-Wall is OFF. With Auto-Wall ON we never call
// this — the aimbot has no occlusion test, so it naturally aims through walls.
static bool IsEnemyVisible(C_CSPlayerPawn* pawn)
{
    __try {
        // m_bSpotted is at +0x8 WITHIN EntitySpottedState_t, not at its base.
        return *reinterpret_cast<bool*>(
            reinterpret_cast<uintptr_t>(pawn) + Offsets::m_entitySpottedState + 0x8);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return true; }
}

// ─── "Am I blinded right now?" ────────────────────────────────────────────────
// IMPORTANT: m_flFlashMaxAlpha holds the PEAK overlay alpha (~255) and the engine
// overwrites it between FSN stages — it does NOT decay back to 0 when the flash
// ends, so reading it makes "disable on flash" permanently true once you've been
// flashed even once. m_flFlashDuration is the live blind signal: it's >0 only
// while actually blinded and 0 otherwise (NoFlash zeroes it to cure blindness).
static bool IsFlashedNow(C_CSPlayerPawn* local)
{
    __try {
        float dur = *reinterpret_cast<float*>(
            reinterpret_cast<uintptr_t>(local) + Offsets::m_flFlashDuration);
        return dur > 0.0f;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ─── Min-damage / hit-chance gates ─────────────────────────────────────────────
// Both default to 0 = "auto" = no restriction (the menu shows "auto" at 0).
//   hit_chance : accuracy gate via player movement speed. Moving spreads bullets,
//                so a higher required chance forces you to be more still to fire.
//   min_damage : generic rifle falloff estimate by distance. No per-weapon damage
//                tables in this project, so it's an approximation, not exact.
static bool PassesGates(C_CSPlayerPawn* local, C_CSPlayerPawn* tgt)
{
    __try {
        if (Globals::aimbot_hit_chance > 0.f && local) {
            Vector vel = *reinterpret_cast<Vector*>(
                reinterpret_cast<uintptr_t>(local) + Offsets::m_vecVelocity);
            float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
            // chance 100 → must be ~still (≤12 u/s); chance 0 → up to 250 u/s
            float maxSpeed = (100.f - Globals::aimbot_hit_chance) * 2.5f + 12.f;
            if (speed > maxSpeed) return false;
        }
        if (Globals::aimbot_min_damage > 0.f && local && tgt) {
            Vector a = local->m_vOldOrigin(), b = tgt->m_vOldOrigin();
            float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            float dist = sqrtf(dx * dx + dy * dy + dz * dz);
            // generic AK-ish base 35 dmg with ~0.85 falloff per 500 units
            float dmg = 35.f * powf(0.85f, dist / 500.f);
            if (dmg < Globals::aimbot_min_damage) return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return true; }
    return true;
}

// ─── Triggerbot: primary crosshair-ID resolution ──────────────────────────────
// Separate function because it uses __try — MSVC C2712 forbids __try in functions
// that have objects with destructors (e.g. GetEntities() vector).
static C_CSPlayerPawn* TriggerbotCrosshair(C_CSPlayerPawn* local)
{
    __try {
        uint32_t id = *reinterpret_cast<uint32_t*>(
            reinterpret_cast<uintptr_t>(local) + Offsets::m_nCrosshairId);

        if (id == 0 || id == 0xFFFFFFFFu) return nullptr;

        // Try direct pawn lookup first, then controller→pawn path
        C_CSPlayerPawn* p = EntityManager::Get().GetPawnFromHandle(id);
        if (!p) {
            uintptr_t ctrl = EntityManager::Get().GetEntityFromHandle(id);
            if (ctrl) {
                uint32_t ph = *reinterpret_cast<uint32_t*>(ctrl + Offsets::m_hPlayerPawn);
                if (ph && ph != 0xFFFFFFFFu)
                    p = EntityManager::Get().GetPawnFromHandle(ph);
            }
        }
        if (!p || p == local || !p->IsAlive()) return nullptr;

        uint8_t myT = *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(local) + Offsets::m_iTeamNum);
        uint8_t thT = *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(p)     + Offsets::m_iTeamNum);

        bool isEnemy   = (myT != thT);
        bool isFriendly = (myT == thT);

        if (isEnemy) {
            // Auto-wall OFF: only fire at visible enemies
            if (!Globals::aimbot_auto_wall && !IsEnemyVisible(p)) return nullptr;
            return p;
        }
        if (isFriendly && Globals::aimbot_friendly_fire) {
            return p;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return nullptr;
}

// ─── Triggerbot target test ───────────────────────────────────────────────────
// Primary: m_nCrosshairId (engine tells us what the dot is over).
// Fallback: bone+FOV scan (covers cases where the handle doesn't resolve as pawn).
// This function holds the GetEntities() vector, so it must have NO __try.
static C_CSPlayerPawn* TriggerbotOnTarget(C_CSPlayerPawn* local,
                                           const Vector& eye, const Vector& view)
{
    // ── Primary: engine crosshair ID ─────────────────────────────────────────
    if (C_CSPlayerPawn* p = TriggerbotCrosshair(local))
        return p;

    // ── Fallback: bone+FOV scan ───────────────────────────────────────────────
    // Wider FOV (5°) than aimbot because we're not moving the mouse here;
    // the user is aiming manually and we just need to confirm "on target".
    // Head bone in CS2 (bone 6) sits slightly below the actual head hitbox,
    // so a tighter window silently misses the exact head hitbox position.
    bool anyHb = false;
    for (int i = 0; i < 6; ++i) if (Globals::triggerbot_hitboxes[i]) { anyHb = true; break; }

    for (const auto& e : EntityManager::Get().GetEntities()) {
        if (!e.pawn || !e.pawn->IsAlive()) continue;

        // Team filter: skip teammates unless friendly-fire is on
        if (!e.isEnemy && !Globals::aimbot_friendly_fire) continue;

        // Wall check: only fire through walls when auto-wall is on
        if (!Globals::aimbot_auto_wall && !IsEnemyVisible(e.pawn)) continue;

        for (int i = 0; i < 6; ++i) {
            if (anyHb && !Globals::triggerbot_hitboxes[i]) continue;
            Vector bp;
            if (!Utils::SafeReadBonePos(e.pawn, kBoneMap[i], bp)) continue;
            // Reject only fully-zero positions (unloaded bone); small values are valid
            if (bp.x == 0.f && bp.y == 0.f && bp.z == 0.f) continue;
            Vector ang = Utils::CalcAngle(eye, bp);
            Utils::NormalizeAngles(ang);
            if (AngleFov(view, ang) < 5.f) return e.pawn;
        }
    }
    return nullptr;
}

// ─── Triggerbot ──────────────────────────────────────────────────────────────
// Holds the fire button DOWN while a target sits under the crosshair, and lets
// go the moment it leaves. This is what makes auto weapons spray continuously
// instead of firing a single bullet per acquire (the old tap behaviour).
static bool s_trigHolding = false;

static void TriggerRelease() {
    if (!s_trigHolding) return;
    INPUT inp = {}; inp.type = INPUT_MOUSE;
    inp.mi.dwFlags = MOUSEEVENTF_LEFTUP; SendInput(1, &inp, sizeof(INPUT));
    s_trigHolding = false;
}

static void RunTriggerbot(C_CSPlayerPawn* local, const Vector& eye, const Vector& view)
{
    if (!Globals::triggerbot_enabled) { s_trigActive = false; TriggerRelease(); return; }

    if (Globals::triggerbot_key != 0 &&
        !(GetAsyncKeyState(Globals::triggerbot_key) & 0x8000))
    { s_trigActive = false; TriggerRelease(); return; }

    // Exclude knives, grenades, Zeus
    uint16_t curDef = GetActiveWeaponDef(local);
    if (curDef && IsExcludedWeapon(curDef)) { s_trigActive = false; TriggerRelease(); return; }

    C_CSPlayerPawn* tgt = TriggerbotOnTarget(local, eye, view);
    if (!tgt) { s_trigActive = false; TriggerRelease(); return; }

    // Respect min-damage / hit-chance gates (shared with the aimbot).
    if (!PassesGates(local, tgt)) { s_trigActive = false; TriggerRelease(); return; }

    auto now = clk::now();
    if (!s_trigActive) {
        s_trigActive = true;
        s_trigStart  = now;   // start the reaction-delay timer
        return;
    }

    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - s_trigStart).count();
    if (ms >= static_cast<long long>(Globals::triggerbot_delay) && !s_trigHolding) {
        // Press and HOLD — auto weapons keep firing while the target stays on
        // the crosshair; semi-autos fire once (the game caps re-fire on hold).
        INPUT inp = {}; inp.type = INPUT_MOUSE;
        inp.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; SendInput(1, &inp, sizeof(INPUT));
        s_trigHolding = true;
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────
bool Aimbot::HasTarget() {
    return s_target != nullptr && s_target->IsAlive();
}

// Target-selection loop lives in its own function: it materialises the
// GetEntities() snapshot (a std::vector with a destructor), which cannot sit
// inside a function that also uses __try (MSVC C2712).
static C_CSPlayerPawn* SelectBestTarget(const Vector& eye, const Vector& view,
                                        Vector& outPos, float& outFov)
{
    C_CSPlayerPawn* bestPawn = nullptr;
    float  bestFov = Globals::aimbot_fov;
    Vector bestPos; float bfv = 0.f;
    for (const auto& e : EntityManager::Get().GetEntities()) {
        if (!e.pawn || !e.pawn->IsAlive()) continue;
        if (!Globals::aimbot_friendly_fire && !e.isEnemy) continue;
        // Visibility gate: skip enemies behind walls unless auto-wall is ON.
        // Only applies to ENEMIES — teammates are never "spotted" (m_bSpotted is
        // false for friendlies), so applying it to them would silently exclude
        // teammates from friendly-fire silent aim whenever auto-wall is off.
        if (e.isEnemy && !Globals::aimbot_auto_wall && !IsEnemyVisible(e.pawn)) continue;
        Vector bp; float fv;
        if (!BestBoneForTarget(e.pawn, eye, view, bestFov, bp, fv)) continue;
        bestFov = fv; bestPawn = e.pawn; bestPos = bp; bfv = fv;
    }
    if (bestPawn) { outPos = bestPos; outFov = bfv; }
    return bestPawn;
}

// ─── Silent Aim (independent feature) ─────────────────────────────────────────
// Standalone — NOT tied to the visual aimbot. Own enable (silent_enabled) + own
// key (silent_key). Shares FOV/hitboxes with the active profile (flat globals,
// already applied by ApplyActiveProfile this frame). Only steers the bullet
// while LMB is held: stores the target angle in Aimbot::g_silent* and the
// CCSGOInput::CreateMove hook injects it into the subtick frame history without
// touching the camera. Has its own __try-free GetEntities scope (C2712).
static void RunSilentAim(C_CSPlayerPawn* local, const Vector& eye, const Vector& view)
{
    Aimbot::g_silentValid = false;   // default: nothing to inject this frame

    if (!Globals::silent_enabled) return;
    if (IsAnyMenuOpen()) return;

    // Only fire-assist while the user is actually shooting (LMB held).
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;
    // Optional extra hold-key gate.
    if (Globals::silent_key != 0 && !(GetAsyncKeyState(Globals::silent_key) & 0x8000)) return;

    // Exclude knives, grenades, Zeus.
    uint16_t curDef = GetActiveWeaponDef(local);
    if (curDef && IsExcludedWeapon(curDef)) return;

    // Disable-when-flashed also gates silent aim (shares the aimbot option).
    if (Globals::aimbot_disable_on[1] && IsFlashedNow(local)) return;

    Vector pos; float fov = 0.f;
    C_CSPlayerPawn* tgt = SelectBestTarget(eye, view, pos, fov);
    if (!tgt) return;

    Vector ang = Utils::CalcAngle(eye, pos);
    Utils::NormalizeAngles(ang);
    Aimbot::g_silentPitch = ang.x;
    Aimbot::g_silentYaw   = ang.y;
    Aimbot::g_silentValid = true;
}

void Aimbot::Run(int stage)
{
    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;

    // ── Stage 7: no longer used for silent aim (angle never written to view) ──
    if (stage == 7) {
        return;
    }

    if (stage != 0) return;

    // ── Delta time ────────────────────────────────────────────────────────────
    auto now = clk::now();
    float dt = std::chrono::duration_cast<std::chrono::microseconds>(
                   now - s_lastFrame).count() / 1e6f;
    s_lastFrame = now;
    dt = std::clamp(dt, 0.0005f, 0.05f);  // clamp 20fps–2000fps range

    // Always record backtrack regardless of aimbot being on
    RecordBacktrack();

    // Apply the per-weapon profile, then run the INDEPENDENT combat helpers
    // (RCS / Triggerbot / Auto-Pistol). These each have their own enable + key +
    // weapon gates, so they must run regardless of whether the aimbot is enabled
    // or its key is held — otherwise triggerbot/RCS would only work while the
    // aimbot key is pressed.
    {
        auto* local0 = EntityManager::Get().GetLocalPawn();
        if (local0 && local0->IsAlive()) {
            ApplyActiveProfile(local0);
            __try {
                Vector eye  = local0->m_vOldOrigin() + local0->m_vecViewOffset();
                Vector view = ReadViewAngles(client);
                RCS::UpdateShots(local0);
                RunTriggerbot(local0, eye, view);
                RunAutoPistol(local0);
                // Rage owns the silent channel when enabled; otherwise the legit
                // silent aim does. They never run together so they can't fight
                // over Aimbot::g_silent*.
                if (Globals::rage_enabled)
                    Rage::Run(local0, eye, view);
                else
                    RunSilentAim(local0, eye, view);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }

    // Visual aimbot is disabled while the independent Silent Aim or Rage is on
    // (both replace the moving aim with bullet steering). They still run above.
    if (!Globals::aimbot_enabled || Globals::silent_enabled || Globals::rage_enabled) {
        s_target = nullptr; s_smoothValid = false;
        return;
    }

    __try {
        auto* local = EntityManager::Get().GetLocalPawn();
        if (!local || !local->IsAlive()) {
            s_target = nullptr; s_smoothValid = false; Aimbot::g_silentValid = false; return;
        }

        // Exclude knives, grenades, Zeus from aimbot
        {
            uint16_t curDef = GetActiveWeaponDef(local);
            if (curDef && IsExcludedWeapon(curDef)) {
                s_target = nullptr; s_smoothValid = false; Aimbot::g_silentValid = false; return;
            }
        }

        // Flash gate: aimbot_disable_on[1] = disable when actually blinded.
        // Uses m_flFlashDuration (live signal) — NOT m_flFlashMaxAlpha, which
        // stays at its peak (~255) and would disable the aim permanently after
        // the first flash even once you can see again.
        if (Globals::aimbot_disable_on[1] && IsFlashedNow(local)) {
            s_target = nullptr; s_smoothValid = false; Aimbot::g_silentValid = false; return;
        }

        // Key gate
        if (Globals::aimbot_key != 0) {
            if (!(GetAsyncKeyState(Globals::aimbot_key) & 0x8000)) {
                s_target = nullptr; s_smoothValid = false; Aimbot::g_silentValid = false; return;
            }
        }

        Vector eye  = local->m_vOldOrigin() + local->m_vecViewOffset();
        Vector view = ReadViewAngles(client);

        // Auto-scope: if targeting a scoped weapon that isn't scoped yet, inject zoom.
        // We inject right-click via SendInput (works for zoom in CS2 as a button event).
        if (Globals::aimbot_auto_scope) {
            __try {
                bool isScoped = *reinterpret_cast<bool*>(
                    reinterpret_cast<uintptr_t>(local) + Offsets::m_bIsScoped);
                uint16_t wDef = GetActiveWeaponDef(local);
                // Scoped rifles: AWP(9), SSG08(40), SG553(39), AUG(8), SCAR20(38), G3SG1(11)
                bool isScopedWeapon = (wDef==9||wDef==40||wDef==39||wDef==8||wDef==38||wDef==11);
                if (isScopedWeapon && !isScoped) {
                    static clk::time_point s_lastScope = clk::now();
                    auto nowS = clk::now();
                    long long msSince = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           nowS - s_lastScope).count();
                    if (msSince > 200) {
                        s_lastScope = nowS;
                        INPUT inp[2] = {};
                        inp[0].type = INPUT_MOUSE; inp[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                        inp[1].type = INPUT_MOUSE; inp[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
                        SendInput(2, inp, sizeof(INPUT));
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        // (Silent aim is handled by the independent RunSilentAim above — the
        //  visual aimbot below is pure smooth/snap camera movement.)

        // Validate locked target
        if (s_target && !s_target->IsAlive()) {
            s_target = nullptr;
            s_lastKillTime = now;
            s_hadTarget    = true;
            s_smoothValid  = false;
        }

        // Kill delay gate
        if (s_hadTarget && Globals::aimbot_kill_delay > 0.f) {
            long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now - s_lastKillTime).count();
            if (ms < static_cast<long long>(Globals::aimbot_kill_delay * 1000.f)) {
                s_smoothValid = false; return;
            }
            s_hadTarget = false;
        }

        // Find best target
        Vector targetPos;
        float  targetFov = 0.f;

        if (Globals::aimbot_lock_target && s_target) {
            if (!BestBoneForTarget(s_target, eye, view, Globals::aimbot_fov * 3.f,
                                    targetPos, targetFov))
            { s_target = nullptr; s_smoothValid = false; }
        }

        if (!s_target) {
            Vector bestPos; float bfv = 0.f;
            C_CSPlayerPawn* bestPawn = SelectBestTarget(eye, view, bestPos, bfv);
            if (bestPawn) {
                if (bestPawn != s_target) {
                    s_target      = bestPawn;
                    s_targetSince = now;
                    s_smoothValid = false; // reset smooth on new target
                }
                targetPos = bestPos; targetFov = bfv;
            }
        }

        if (!s_target) { s_smoothValid = false; return; }

        // Shot delay gate
        long long msAcquire = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now - s_targetSince).count();
        if (msAcquire < static_cast<long long>(Globals::aimbot_shot_delay * 1000.f)) {
            s_smoothValid = false; return;
        }

        // Target angle — NO recoil comp in smooth path (use RCS for that)
        Vector aimAng = Utils::CalcAngle(eye, targetPos);
        Utils::NormalizeAngles(aimAng);

        // ─ Time-based smooth interpolation ───────────────────────────────────
        if (!s_smoothValid) {
            s_smoothAngle = view;
            s_smoothValid = true;
        } else {
            // Detect external view-angle jump (recoil, punch) and re-sync.
            // If view moved more than 5° away from what we wrote last frame,
            // the game modified the angle on its own (recoil / punch).
            Vector chk = { view.x - s_smoothAngle.x, view.y - s_smoothAngle.y, 0.f };
            Utils::NormalizeAngles(chk);
            if (sqrtf(chk.x*chk.x + chk.y*chk.y) > 5.f)
                s_smoothAngle = view;
        }

        float smooth    = std::max(1.f, Globals::aimbot_smooth);
        float speed     = 360.f / smooth;  // deg/s
        float maxStep   = speed * dt;

        Vector delta = { aimAng.x - s_smoothAngle.x, aimAng.y - s_smoothAngle.y, 0.f };
        Utils::NormalizeAngles(delta);
        float dist = sqrtf(delta.x * delta.x + delta.y * delta.y);

        if (dist < 0.01f) {
            s_smoothAngle = aimAng;
        } else if (dist > maxStep) {
            float t = maxStep / dist;
            s_smoothAngle.x += delta.x * t;
            s_smoothAngle.y += delta.y * t;
        } else {
            s_smoothAngle = aimAng;
        }
        Utils::NormalizeAngles(s_smoothAngle);

        WriteViewAngles(client, s_smoothAngle);
        WriteEyeAngles(local, s_smoothAngle);

        // Auto fire — only if smooth aim is very close to target + gates pass
        if (Globals::aimbot_auto_fire && PassesGates(local, s_target)) {
            float onTargetFov = AngleFov(s_smoothAngle, aimAng);
            if (onTargetFov < 0.5f) {
                static clk::time_point s_lastFire = clk::now();
                long long msFire = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       now - s_lastFire).count();
                if (msFire >= 80) {
                    INPUT inp[2] = {};
                    inp[0].type = INPUT_MOUSE; inp[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                    inp[1].type = INPUT_MOUSE; inp[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                    SendInput(2, inp, sizeof(INPUT));
                    s_lastFire = now;
                }
            }
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_target = nullptr; s_smoothValid = false;
    }
}

// Called from Hooks.cpp FSN stage 5: punch is valid after prediction.
void Aimbot::RunRCS_Stage5()
{
    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;
    auto* local = EntityManager::Get().GetLocalPawn();
    if (!local || !local->IsAlive()) return;
    __try { RCS::RunAtStage5(local, client); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
