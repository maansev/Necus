#define NOMINMAX
#include "Visuals.h"
#include "esp/Esp.h"
#include "enemycounter/EnemyCounter.h"
#include "chams/Chams.h"
#include "../../../ext/imgui/imgui.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/utils/Utils.h"
#include "../../sdk/utils/Vector.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/memory/Offsets.h"
#include "../../sdk/memory/PatternScan.h"
#include <cmath>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <Windows.h>

// ─── SEH-safe helpers (plain functions, no C++ objects — avoids C2712) ───────
static bool SafeReadBool(uintptr_t addr, bool& out) {
    if (!addr) return false;
    __try { out = *reinterpret_cast<bool*>(addr); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool SafeReadPtr(uintptr_t addr, uintptr_t& out) {
    if (!addr) return false;
    __try { out = *reinterpret_cast<uintptr_t*>(addr); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool SafeReadI32(uintptr_t addr, int32_t& out) {
    if (!addr) return false;
    __try { out = *reinterpret_cast<int32_t*>(addr); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool SafeReadF32(uintptr_t addr, float& out) {
    if (!addr) return false;
    __try { out = *reinterpret_cast<float*>(addr); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool SafeReadVec(uintptr_t addr, Vector& out) {
    if (!addr) return false;
    __try { out = *reinterpret_cast<Vector*>(addr); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool SafeWriteU8(uintptr_t addr, uint8_t v) {
    if (!addr) return false;
    __try { *reinterpret_cast<uint8_t*>(addr) = v; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool SafeWriteF32(uintptr_t addr, float v) {
    if (!addr) return false;
    __try { *reinterpret_cast<float*>(addr) = v; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool SafeReadU32(uintptr_t addr, uint32_t& out) {
    if (!addr) return false;
    __try { out = *reinterpret_cast<uint32_t*>(addr); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Local pawn address straight from client.dll (not the cached EntityManager copy)
static uintptr_t GetLocalPawnAddr() {
    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return 0;
    uintptr_t pawn = 0;
    if (!SafeReadPtr(client + Offsets::dwLocalPlayerPawn, pawn)) return 0;
    return pawn;
}

// Eye position + forward direction of the local player
static bool GetLocalEyeAndDir(Vector& eye, Vector& dir) {
    uintptr_t client = Memory::GetModuleBase("client.dll");
    uintptr_t pawn = GetLocalPawnAddr();
    if (!client || !pawn) return false;

    Vector origin{}, viewOff{};
    if (!SafeReadVec(pawn + Offsets::m_vOldOrigin, origin)) return false;
    if (!SafeReadVec(pawn + Offsets::m_vecViewOffset, viewOff)) return false;
    eye = { origin.x + viewOff.x, origin.y + viewOff.y, origin.z + viewOff.z };

    float pitch = 0.f, yaw = 0.f;
    if (!SafeReadF32(client + Offsets::dwViewAngles + 0, pitch)) return false;
    if (!SafeReadF32(client + Offsets::dwViewAngles + 4, yaw))   return false;

    const float D2R = 3.14159265f / 180.f;
    float cp = cosf(pitch * D2R), sp = sinf(pitch * D2R);
    float cy = cosf(yaw * D2R), sy = sinf(yaw * D2R);
    dir = { cp * cy, cp * sy, -sp };
    return true;
}

// ─── Removals ────────────────────────────────────────────────────────────────
namespace Removals {

    static bool s_legsHidden = false;

    // ── Scope overlay removal ─────────────────────────────────────────────────
    // CS2 draws the black scope vignette only while m_bIsScoped is true at
    // render time. Zero it right before the frame renders (FRAME_RENDER_START)
    // and restore right after (stage 7) — zoom (FOV) is server-driven and
    // unaffected, but the overlay/blur never draws.
    static bool s_scopedWasTrue = false;

    void OnRenderStart() {
        s_scopedWasTrue = false;
        if (!Globals::misc_remove_scope_overlay) return;
        uintptr_t pawn = GetLocalPawnAddr();
        if (!pawn) return;
        bool scoped = false;
        if (SafeReadBool(pawn + Offsets::m_bIsScoped, scoped) && scoped) {
            SafeWriteU8(pawn + Offsets::m_bIsScoped, 0);
            s_scopedWasTrue = true;
        }
    }

    void OnRenderEnd() {
        if (!s_scopedWasTrue) return;
        uintptr_t pawn = GetLocalPawnAddr();
        if (pawn) SafeWriteU8(pawn + Offsets::m_bIsScoped, 1);
        s_scopedWasTrue = false;
    }

    // ── Crosshair removal: hide the built-in CS2 crosshair via HideHUD bits ──
    static bool s_hudBitSet = false;

    static void UpdateCrosshair() {
        uintptr_t pawn = GetLocalPawnAddr();
        if (!pawn) return;

        int32_t hud = 0;
        if (!SafeReadI32(pawn + Offsets::m_iHideHUD, hud)) return;
        constexpr int32_t HIDEHUD_CROSSHAIR = 1 << 8;  // 256

        if (Globals::misc_remove_crosshair) {
            if (!(hud & HIDEHUD_CROSSHAIR)) {
                __try { *reinterpret_cast<int32_t*>(pawn + Offsets::m_iHideHUD) = hud | HIDEHUD_CROSSHAIR; }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            s_hudBitSet = true;
        }
        else if (s_hudBitSet) {
            __try { *reinterpret_cast<int32_t*>(pawn + Offsets::m_iHideHUD) = hud & ~HIDEHUD_CROSSHAIR; }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
            s_hudBitSet = false;
        }
    }

    // ── Team intro skip ("Первая половина" заставка) ─────────────────────────
    // CCSGameRules::m_bTeamIntroPeriod = false завершает интро мгновенно.
    // Активно только когда оба оффсета (dwGameRules / m_bTeamIntroPeriod)
    // заполнены из дампа.
    static void UpdateTeamIntro() {
        if (!Globals::misc_remove_team_intro) return;
        if (!Offsets::dwGameRules || !Offsets::m_bTeamIntroPeriod) return;
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return;
        uintptr_t rules = 0;
        if (!SafeReadPtr(client + Offsets::dwGameRules, rules) || !rules) return;
        bool intro = false;
        if (SafeReadBool(rules + Offsets::m_bTeamIntroPeriod, intro) && intro)
            SafeWriteU8(rules + Offsets::m_bTeamIntroPeriod, 0);
    }

    // Per-frame memory writes (legs / shadows) — independent of scope state
    static void UpdateWorld() {
        UpdateCrosshair();
        UpdateTeamIntro();

        // Legs: zero the alpha of the local player's world model so you don't
        // see your own legs/body when looking down. Restore on disable.
        //
        // KNOWN LIMITATION: in CS2 the first-person legs are drawn by the world
        // model pass controlled by the convar `cl_first_person_uses_world_model`
        // (and the render alpha is recomputed by the engine), so a raw
        // m_clrRender alpha write is frequently overwritten / ignored for the
        // local pawn. Without an ICvar interface (removed — it froze the game)
        // there is no reliable convar path; the alpha write below is best-effort
        // and is re-applied every Present frame via Visuals::UpdateRemovals().
        uintptr_t pawn = GetLocalPawnAddr();

        // Scope overlay: also zero every frame as belt-and-suspenders
        if (Globals::misc_remove_scope_overlay && pawn) {
            bool sc = false;
            if (SafeReadBool(pawn + Offsets::m_bIsScoped, sc) && sc)
                SafeWriteU8(pawn + Offsets::m_bIsScoped, 0);
        }
        if (pawn) {
            if (Globals::misc_remove_legs) {
                // Primary: GameSceneNode scale ≈ 0 — engine skips drawing the
                // local world model (legs) entirely. Needs m_flScale offset.
                if (Offsets::m_flScale) {
                    uintptr_t node = 0;
                    if (SafeReadPtr(pawn + Offsets::m_pGameSceneNode, node) && node)
                        SafeWriteF32(node + Offsets::m_flScale, 0.0001f);
                }
                // Fallback: render alpha (часто игнорируется движком для
                // локального пешки, но безвредно)
                SafeWriteU8(pawn + Offsets::m_clrRender + 3, 0);
                s_legsHidden = true;
            }
            else if (s_legsHidden) {
                if (Offsets::m_flScale) {
                    uintptr_t node = 0;
                    if (SafeReadPtr(pawn + Offsets::m_pGameSceneNode, node) && node)
                        SafeWriteF32(node + Offsets::m_flScale, 1.f);
                }
                SafeWriteU8(pawn + Offsets::m_clrRender + 3, 255);
                s_legsHidden = false;
            }
        }

        // Shadows: kill shadow strength on all player pawns; restore on disable.
        static bool s_shadowsKilled = false;
        if (Globals::misc_remove_shadows) {
            const auto& ents = EntityManager::Get().GetEntities();
            for (const auto& ent : ents)
                SafeWriteF32(reinterpret_cast<uintptr_t>(ent.pawn) + Offsets::m_flShadowStrength, 0.f);
            if (pawn)
                SafeWriteF32(pawn + Offsets::m_flShadowStrength, 0.f);
            s_shadowsKilled = true;
        }
        else if (s_shadowsKilled) {
            const auto& ents = EntityManager::Get().GetEntities();
            for (const auto& ent : ents)
                SafeWriteF32(reinterpret_cast<uintptr_t>(ent.pawn) + Offsets::m_flShadowStrength, 1.f);
            if (pawn)
                SafeWriteF32(pawn + Offsets::m_flShadowStrength, 1.f);
            s_shadowsKilled = false;
        }
    }

} // namespace Removals

// Exported for Hooks.cpp (FrameStageNotify stage 5 / stage 7).
// All removals are memory writes timed to the render frame: crosshair via
// HideHUD, scope overlay via m_bIsScoped toggle, legs/shadows via UpdateWorld.
void Visuals::OnRenderStart() {
    Removals::UpdateWorld();
    Removals::OnRenderStart();
}
void Visuals::OnRenderEnd() {
    Removals::OnRenderEnd();
}
void Visuals::UpdateRemovals() {
    Removals::UpdateWorld();
}

// ─── Hit marker: bullet tracer + hit cross on the target ────────────────────
namespace HitMarker {

    struct Tracer { Vector start, end; double t; };
    struct Hit { Vector pos;        double t; };

    static std::vector<Tracer>          s_tracers;
    static std::vector<Hit>             s_hits;
    static std::unordered_map<int, int>  s_prevHealth;
    static std::unordered_map<int, int>  s_prevEntShots;   // per-entity m_iShotsFired
    static int                          s_prevShots = 0;
    static double                       s_lastShotTime = -10.0;

    // Eye position + view direction of an arbitrary pawn (for other players' tracers)
    static bool GetPawnEyeAndDir(uintptr_t pa, Vector& eye, Vector& dir) {
        Vector origin{}, viewOff{};
        if (!SafeReadVec(pa + Offsets::m_vOldOrigin, origin)) return false;
        if (!SafeReadVec(pa + Offsets::m_vecViewOffset, viewOff)) return false;
        eye = { origin.x + viewOff.x, origin.y + viewOff.y, origin.z + viewOff.z };

        float pitch = 0.f, yaw = 0.f;
        if (!SafeReadF32(pa + Offsets::m_angEyeAngles + 0, pitch)) return false;
        if (!SafeReadF32(pa + Offsets::m_angEyeAngles + 4, yaw))   return false;

        const float D2R = 3.14159265f / 180.f;
        float cp = cosf(pitch * D2R), sp = sinf(pitch * D2R);
        float cy = cosf(yaw * D2R), sy = sinf(yaw * D2R);
        dir = { cp * cy, cp * sy, -sp };
        return true;
    }

    static void AddTracer(const Vector& eye, const Vector& dir, double now) {
        Tracer tr;
        tr.start = { eye.x + dir.x * 60.f,   eye.y + dir.y * 60.f,   eye.z + dir.z * 60.f - 4.f };
        tr.end = { eye.x + dir.x * 4000.f, eye.y + dir.y * 4000.f, eye.z + dir.z * 4000.f };
        tr.t = now;
        s_tracers.push_back(tr);
    }

    static void Update() {
        double now = ImGui::GetTime();

        // 1) Own shots: m_iShotsFired increases while firing → tracer + arm hit window
        uintptr_t pawn = GetLocalPawnAddr();
        if (pawn) {
            int shots = 0;
            if (SafeReadI32(pawn + Offsets::m_iShotsFired, shots)) {
                if (shots > s_prevShots) {
                    s_lastShotTime = now;
                    Vector eye{}, dir{};
                    if (GetLocalEyeAndDir(eye, dir))
                        AddTracer(eye, dir, now);
                }
                s_prevShots = shots;
            }
        }

        // 2) Other players' shots → tracers, filtered by Teammates/Enemies toggles
        const auto& ents = EntityManager::Get().GetEntities();
        {
            std::unordered_map<int, int> freshShots;
            freshShots.reserve(ents.size());
            for (const auto& ent : ents) {
                uintptr_t pa = reinterpret_cast<uintptr_t>(ent.pawn);
                int shots = 0;
                if (!SafeReadI32(pa + Offsets::m_iShotsFired, shots)) continue;

                auto it = s_prevEntShots.find(ent.index);
                if (it != s_prevEntShots.end() && shots > it->second) {
                    bool want = ent.isEnemy ? Globals::misc_hit_marker_tracer_enemy
                        : Globals::misc_hit_marker_tracer_team;
                    if (want) {
                        Vector eye{}, dir{};
                        if (GetPawnEyeAndDir(pa, eye, dir))
                            AddTracer(eye, dir, now);
                    }
                }
                freshShots[ent.index] = shots;
            }
            s_prevEntShots = std::move(freshShots);
        }

        // 3) Own hits: HP dropped within 350ms of our shot → "+" cross at the target
        std::unordered_map<int, int> fresh;
        fresh.reserve(ents.size());
        for (const auto& ent : ents) {
            uintptr_t pa = reinterpret_cast<uintptr_t>(ent.pawn);
            int hp = 0;
            if (!SafeReadI32(pa + Offsets::m_iHealth, hp)) continue;

            auto it = s_prevHealth.find(ent.index);
            if (it != s_prevHealth.end() && it->second > 0 && hp < it->second
                && (now - s_lastShotTime) < 0.35) {
                Vector origin{}, viewOff{};
                if (SafeReadVec(pa + Offsets::m_vOldOrigin, origin)) {
                    SafeReadVec(pa + Offsets::m_vecViewOffset, viewOff);
                    s_hits.push_back({ { origin.x, origin.y, origin.z + viewOff.z * 0.8f }, now });
                }
            }
            fresh[ent.index] = hp;
        }
        s_prevHealth = std::move(fresh);
    }

    static void Render() {
        if (!Globals::misc_hit_marker) return;

        Update();

        double now = ImGui::GetTime();
        float  dur = Globals::misc_hit_marker_duration;
        const float* c = Globals::misc_hit_marker_color;
        const float sw = ImGui::GetIO().DisplaySize.x;
        const float sh = ImGui::GetIO().DisplaySize.y;

        s_tracers.erase(std::remove_if(s_tracers.begin(), s_tracers.end(),
            [&](const Tracer& t) { return now - t.t >= dur; }), s_tracers.end());
        s_hits.erase(std::remove_if(s_hits.begin(), s_hits.end(),
            [&](const Hit& h) { return now - h.t >= dur; }), s_hits.end());

        if (s_tracers.empty() && s_hits.empty()) return;

        ImDrawList* dl = ImGui::GetForegroundDrawList();

        // Bullet tracers — fading world-space lines along the shot direction
        for (const auto& tr : s_tracers) {
            float alpha = 1.f - (float)((now - tr.t) / dur);
            Vector s1{}, s2{};
            if (Utils::WorldToScreen(tr.start, s1, (float*)Globals::ViewMatrix, sw, sh) &&
                Utils::WorldToScreen(tr.end, s2, (float*)Globals::ViewMatrix, sw, sh)) {
                ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * alpha));
                dl->AddLine({ s1.x, s1.y }, { s2.x, s2.y }, col, 1.6f);
            }
        }

        // Hit markers — clean "+" cross with shadow, pinned to world hit position
        for (const auto& h : s_hits) {
            float t = (float)((now - h.t) / dur);
            float alpha = (1.f - t) * (1.f - t); // quadratic fade (sharper)
            Vector sc{};
            if (!Utils::WorldToScreen(h.pos, sc, (float*)Globals::ViewMatrix, sw, sh)) continue;

            // gap expands slightly as it fades out
            float gap = Globals::misc_hit_marker_size * 0.5f;
            float arm = Globals::misc_hit_marker_size * (1.f + t * 0.3f);

            ImU32 shadow = IM_COL32(0, 0, 0, (int)(180 * alpha));
            ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * alpha));

            // shadow (1px offset, slightly thicker)
            dl->AddLine({ sc.x - gap - arm + 1, sc.y + 1 }, { sc.x - gap + 1, sc.y + 1 }, shadow, 1.6f);
            dl->AddLine({ sc.x + gap + 1,       sc.y + 1 }, { sc.x + gap + arm + 1, sc.y + 1 }, shadow, 1.6f);
            dl->AddLine({ sc.x + 1, sc.y - gap - arm + 1 }, { sc.x + 1, sc.y - gap + 1 }, shadow, 1.6f);
            dl->AddLine({ sc.x + 1, sc.y + gap + 1 }, { sc.x + 1, sc.y + gap + arm + 1 }, shadow, 1.6f);

            // coloured "+" arms
            dl->AddLine({ sc.x - gap - arm, sc.y }, { sc.x - gap, sc.y }, col, 1.4f);
            dl->AddLine({ sc.x + gap,       sc.y }, { sc.x + gap + arm, sc.y }, col, 1.4f);
            dl->AddLine({ sc.x, sc.y - gap - arm }, { sc.x, sc.y - gap }, col, 1.4f);
            dl->AddLine({ sc.x, sc.y + gap }, { sc.x, sc.y + gap + arm }, col, 1.4f);
        }
    }

} // namespace HitMarker

// ─── Damage log ───────────────────────────────────────────────────────────────
// Poll-based detection (no game-event listener available): mirrors the hit
// marker's HP-drop tracking. When an enemy's HP drops within 350ms of our own
// m_iShotsFired increment, we log "Hit <name> for <dmg> (<hp> hp)". Hitbox is
// unknown without events, so it is omitted.
static bool SafeCopyStr(uintptr_t addr, char* out, size_t n) {
    if (!addr || !out || !n) return false;
    __try {
        size_t i = 0;
        const char* src = reinterpret_cast<const char*>(addr);
        for (; i < n - 1 && src[i]; ++i) out[i] = src[i];
        out[i] = '\0';
        return i > 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = '\0'; return false; }
}

namespace DamageLog {

    static std::vector<Visuals::DmgEntry>  s_log;
    static std::unordered_map<int, int>    s_prevHealth;
    static int                             s_prevShots = 0;
    static double                          s_lastShotTime = -10.0;

    static void Update() {
        double now = ImGui::GetTime();

        uintptr_t pawn = GetLocalPawnAddr();
        if (pawn) {
            int shots = 0;
            if (SafeReadI32(pawn + Offsets::m_iShotsFired, shots)) {
                if (shots > s_prevShots) s_lastShotTime = now;
                s_prevShots = shots;
            }
        }

        const auto& ents = EntityManager::Get().GetEntities();
        std::unordered_map<int, int> fresh;
        fresh.reserve(ents.size());
        for (const auto& ent : ents) {
            if (!ent.isEnemy) { continue; }
            uintptr_t pa = reinterpret_cast<uintptr_t>(ent.pawn);
            int hp = 0;
            if (!SafeReadI32(pa + Offsets::m_iHealth, hp)) continue;

            auto it = s_prevHealth.find(ent.index);
            if (it != s_prevHealth.end() && it->second > 0 && hp < it->second
                && (now - s_lastShotTime) < 0.35) {
                Visuals::DmgEntry e{};
                e.dmg = it->second - hp;
                e.hp = hp < 0 ? 0 : hp;
                e.time = now;
                if (!SafeCopyStr(reinterpret_cast<uintptr_t>(ent.controller) + Offsets::m_iszPlayerName,
                    e.name, sizeof(e.name)))
                    strncpy_s(e.name, "Enemy", _TRUNCATE);
                s_log.push_back(e);
            }
            fresh[ent.index] = hp;
        }
        s_prevHealth = std::move(fresh);

        // drop expired (5s) and keep the container small
        s_log.erase(std::remove_if(s_log.begin(), s_log.end(),
            [&](const Visuals::DmgEntry& e) { return now - e.time >= 5.0; }), s_log.end());
        if (s_log.size() > 16)
            s_log.erase(s_log.begin(), s_log.end() - 16);
    }

} // namespace DamageLog

int Visuals::GetDamageLog(Visuals::DmgEntry* out, int maxCount) {
    int n = 0;
    for (auto it = DamageLog::s_log.rbegin(); it != DamageLog::s_log.rend() && n < maxCount; ++it)
        out[n++] = *it;   // newest first
    return n;
}

// ─── Penetration crosshair ───────────────────────────────────────────────────
// Shows whether an enemy is in your line of fire: if the crosshair ray passes
// through/near an enemy body — "can hit/pen" color, otherwise "no" color.
namespace PenXhair {

    static void Render() {
        if (!Globals::misc_penetration_xhair) return;

        Vector eye{}, dir{};
        if (!GetLocalEyeAndDir(eye, dir)) return;

        bool onTarget = false;
        const auto& ents = EntityManager::Get().GetEntities();
        for (const auto& ent : ents) {
            if (!ent.isEnemy) continue;
            uintptr_t pa = reinterpret_cast<uintptr_t>(ent.pawn);
            Vector origin{}, viewOff{};
            if (!SafeReadVec(pa + Offsets::m_vOldOrigin, origin)) continue;
            SafeReadVec(pa + Offsets::m_vecViewOffset, viewOff);
            Vector chest = { origin.x, origin.y, origin.z + viewOff.z * 0.75f };

            // distance from ray (eye, dir) to chest point
            Vector to = { chest.x - eye.x, chest.y - eye.y, chest.z - eye.z };
            float along = to.x * dir.x + to.y * dir.y + to.z * dir.z;
            if (along < 50.f || along > 5000.f) continue;
            Vector closest = { eye.x + dir.x * along, eye.y + dir.y * along, eye.z + dir.z * along };
            float dx = chest.x - closest.x, dy = chest.y - closest.y, dz = chest.z - closest.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            // ~38 units ≈ half a body width + some legroom (covers chest→head band)
            if (distSq < 38.f * 38.f) { onTarget = true; break; }
        }

        const float* c = onTarget ? Globals::misc_pen_yes_color : Globals::misc_pen_no_color;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const float cx = ImGui::GetIO().DisplaySize.x * 0.5f;
        const float cy = ImGui::GetIO().DisplaySize.y * 0.5f;

        // "+" exactly at the original crosshair position (screen centre)
        const float l = 4.5f;
        dl->AddLine({ cx - l, cy }, { cx + l, cy }, col, 2.f);
        dl->AddLine({ cx, cy - l }, { cx, cy + l }, col, 2.f);
    }

} // namespace PenXhair

// ─── Third Person ────────────────────────────────────────────────────────────
// CCSGOInput::m_bInThirdPerson = true — встроенный механизм thirdperson CS2,
// работает для живого игрока, камера управляется движком.
namespace ThirdPerson {
    static bool s_wasEnabled = false;

    static void Update() {
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        // CS2 thirdperson: CCSGOInput::m_bInThirdPerson. Движок сам считает
        // позицию камеры за спиной — это тот же механизм, что и встроенный
        // thirdperson. m_bInThirdPerson не из схемы (0x249, билды 2025-26) —
        // запись SEH-защищена, при неверном оффсете просто нет эффекта.
        // dwCSGOInput — статический ОБЪЕКТ в client.dll (не указатель)
        uintptr_t input = client + Offsets::dwCSGOInput;

        if (Globals::misc_thirdperson) {
            SafeWriteU8(input + Offsets::m_bInThirdPerson, 1);
            s_wasEnabled = true;
        }
        else if (s_wasEnabled) {
            SafeWriteU8(input + Offsets::m_bInThirdPerson, 0);
            s_wasEnabled = false;
        }
    }
} // namespace ThirdPerson

// ─── Visuals::Render ─────────────────────────────────────────────────────────
void Visuals::Render()
{
    ThirdPerson::Update();
    if (Globals::misc_damage_logs) DamageLog::Update();
    ESP::Render();
    ESP::RenderPlayerInfo();   // Show Money — независимо от esp_enabled
    EnemyCounter::Render();
    HitMarker::Render();
    PenXhair::Render();

    // Spread circle: model-based inaccuracy estimate (no m_flInaccuracy offset
    // available — heuristic from movement state + recoil). Reads are SEH-safe.
    if (Globals::misc_spread_circle) {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const float sw = ImGui::GetIO().DisplaySize.x;
        const float sh = ImGui::GetIO().DisplaySize.y;
        float cx = sw * 0.5f, cy = sh * 0.5f;

        // Base inaccuracy from weapon VData (CFiringModeFloat[2]: [0]=normal mode)
        // Fallback constants match an AK-47 roughly (used if weapon read fails).
        float baseStand = 0.008f, baseCrouch = 0.006f, baseJump = 0.07f,
            baseMove = 0.045f, baseFire = 0.006f, baseSpread = 0.0f;

        uintptr_t pawn = GetLocalPawnAddr();
        if (pawn) {
            __try {
                uintptr_t ws = 0;
                if (SafeReadPtr(pawn + Offsets::m_pWeaponServices, ws) && ws) {
                    uint32_t wh = 0;
                    if (SafeReadU32(ws + Offsets::m_hActiveWeapon, wh) && wh && wh != 0xFFFFFFFFu) {
                        uintptr_t client = Memory::GetModuleBase("client.dll");
                        uintptr_t listPtr = *reinterpret_cast<uintptr_t*>(client + Offsets::dwEntityList);
                        uintptr_t entry = *reinterpret_cast<uintptr_t*>(listPtr + 8 * ((wh & 0x7FFF) >> 9) + 16);
                        uintptr_t weap = *reinterpret_cast<uintptr_t*>(entry + 112 * (wh & 0x1FF));
                        if (weap) {
                            // VData pointer is usually at weapon+0x370 (C_CSWeaponBase::m_pVData)
                            // but if that's wrong the SEH will catch it.
                            uintptr_t vd = *reinterpret_cast<uintptr_t*>(weap + 0x370);
                            if (vd) {
                                // CFiringModeFloat: struct{ float v[2] }, [0]=normal
                                SafeReadF32(vd + Offsets::m_flInaccuracyStand + 0, baseStand);
                                SafeReadF32(vd + Offsets::m_flInaccuracyCrouch + 0, baseCrouch);
                                SafeReadF32(vd + Offsets::m_flInaccuracyJump + 0, baseJump);
                                SafeReadF32(vd + Offsets::m_flInaccuracyMove + 0, baseMove);
                                SafeReadF32(vd + Offsets::m_flInaccuracyFire + 0, baseFire);
                                SafeReadF32(vd + Offsets::m_flSpread + 0, baseSpread);
                            }
                        }
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        float inacc = baseStand;
        if (pawn) {
            Vector vel{}; int flags = 0, shots = 0; bool ducked = false;
            SafeReadVec(pawn + Offsets::m_vecVelocity, vel);
            SafeReadI32(pawn + Offsets::m_fFlags, flags);
            SafeReadI32(pawn + Offsets::m_iShotsFired, shots);
            SafeReadBool(pawn + Offsets::m_bDucked, ducked);

            bool onGround = (flags & 1) != 0;
            float speed2d = sqrtf(vel.x * vel.x + vel.y * vel.y);

            if (!onGround) {
                inacc = baseJump;
            }
            else if (ducked) {
                inacc = baseCrouch;
                float moveT = std::clamp(speed2d / 200.f, 0.f, 1.f);
                inacc += (baseMove - inacc) * moveT;
            }
            else {
                inacc = baseStand;
                float moveT = std::clamp(speed2d / 250.f, 0.f, 1.f);
                inacc += (baseMove - inacc) * moveT;
            }
            // Fire penalty scales with recoil (m_iShotsFired decays to 0 in-game)
            // Cap shots to avoid float overflow (m_iShotsFired can be garbage on bad read)
            if (shots < 0) shots = 0; else if (shots > 100) shots = 100;
            if (shots > 0) inacc += shots * baseFire;
        }
        // Add spread as a floor
        inacc = std::max(inacc, baseSpread);
        // Guard against NaN/Inf from bad VData reads
        if (!std::isfinite(inacc) || inacc < 0.f) inacc = baseStand;

        // radius_px = tan(inacc) / tan(fov/2) * (screenH/2), fov = 90 deg
        const float fovH = 90.f * 3.14159265f / 180.f;
        float radius = tanf(inacc) / tanf(fovH * 0.5f) * (sh * 0.5f);
        if (!std::isfinite(radius)) radius = 4.f;
        radius = std::clamp(radius, 3.f, sh * 0.45f);

        // Exponential smoothing across frames to avoid jitter
        static float s_smoothR = 4.f;
        float dt = ImGui::GetIO().DeltaTime;
        float k = 1.f - expf(-dt * 12.f);
        s_smoothR += (radius - s_smoothR) * k;
        if (!std::isfinite(s_smoothR)) s_smoothR = 4.f;
        float r = s_smoothR;

        // Ring: dark halo for contrast + white ring
        dl->AddCircle(ImVec2(cx, cy), r, IM_COL32(0, 0, 0, 70), 64, 2.6f);
        dl->AddCircle(ImVec2(cx, cy), r, IM_COL32(255, 255, 255, 190), 64, 1.0f);
    }

    if (Globals::aimbot_enabled && Globals::aimbot_draw_fov) {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const float sw = ImGui::GetIO().DisplaySize.x;
        const float sh = ImGui::GetIO().DisplaySize.y;
        float radius = tanf(Globals::aimbot_fov * 3.14159265f / 180.f) / 0.75f * (sh * 0.5f);
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
            Globals::aimbot_fov_color[0], Globals::aimbot_fov_color[1],
            Globals::aimbot_fov_color[2], Globals::aimbot_fov_color[3]));
        dl->AddCircle(ImVec2(sw * 0.5f, sh * 0.5f), radius, col, 64, 1.5f);
    }
}
