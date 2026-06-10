#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "Aimbot.h"
#include "../../../sdk/entity/EntityManager.h"
#include "../../../sdk/memory/Offsets.h"
#include "../../../sdk/memory/PatternScan.h"
#include "../../../sdk/utils/Globals.h"
#include "../../../sdk/utils/Utils.h"
#include <Windows.h>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <deque>

using clk = std::chrono::steady_clock;

// ─── Internal offsets ────────────────────────────────────────────────────────
static constexpr ptrdiff_t AB_VIEW_ANGLES = 0x23558B8;

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
static C_CSPlayerPawn* s_target = nullptr;
static float           s_savedAngles[3] = {};
static bool            s_savedValid = false;
static clk::time_point s_targetSince = clk::now();
static clk::time_point s_lastKillTime = clk::now();
static bool            s_hadTarget = false;

// Smooth: persistent interpolated angle
static Vector          s_smoothAngle = {};
static bool            s_smoothValid = false;
static clk::time_point s_lastFrame = clk::now();

// ─── Triggerbot state ─────────────────────────────────────────────────────────
static bool            s_trigActive = false;
static clk::time_point s_trigStart = clk::now();

// ─── Backtrack ───────────────────────────────────────────────────────────────
static constexpr int kBTSlots = 16;

struct BoneSnap {
    Vector               bones[6];
    clk::time_point      t;
};

struct BTHistory {
    BoneSnap records[kBTSlots];
    int      head = 0;
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
        auto& rec = hist.records[hist.head];
        rec.t = now;
        for (int i = 0; i < 6; ++i)
            rec.bones[i] = Utils::GetBonePos(e.pawn, kBoneMap[i]);
        hist.head = (hist.head + 1) % kBTSlots;
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
    auto  now = clk::now();
    float btMs = Globals::backtrack_time;

    bool  found = false;
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
    float best = fovLimit;
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

// ─── Triggerbot ──────────────────────────────────────────────────────────────
static void RunTriggerbot(C_CSPlayerPawn* local, const Vector& eye, const Vector& view)
{
    if (!Globals::triggerbot_enabled) { s_trigActive = false; return; }

    // Key gate (0 = always active when triggerbot is enabled)
    if (Globals::triggerbot_key != 0 &&
        !(GetAsyncKeyState(Globals::triggerbot_key) & 0x8000))
    {
        s_trigActive = false; return;
    }

    // Check if crosshair is on an enemy hitbox (very tight FOV)
    bool onTarget = false;
    const auto& ents = EntityManager::Get().GetEntities();
    for (const auto& e : ents) {
        if (!e.isEnemy || !e.pawn || !e.pawn->IsAlive()) continue;
        for (int i = 0; i < 6; ++i) {
            if (!Globals::aimbot_hitboxes[i]) continue;
            Vector bp = Utils::GetBonePos(e.pawn, kBoneMap[i]);
            if (bp.IsZero()) continue;
            Vector ang = Utils::CalcAngle(eye, bp);
            Utils::NormalizeAngles(ang);
            if (AngleFov(view, ang) < 0.6f) { onTarget = true; break; }
        }
        if (onTarget) break;
    }

    if (!onTarget) { s_trigActive = false; return; }

    auto now = clk::now();
    if (!s_trigActive) {
        s_trigActive = true;
        s_trigStart = now;
        return;
    }

    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - s_trigStart).count();
    if (ms >= static_cast<long long>(Globals::triggerbot_delay)) {
        // Two separate SendInput calls so CS2 sees them as discrete down/up events
        INPUT inp = {}; inp.type = INPUT_MOUSE;
        inp.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; SendInput(1, &inp, sizeof(INPUT));
        inp.mi.dwFlags = MOUSEEVENTF_LEFTUP;   SendInput(1, &inp, sizeof(INPUT));
        s_trigActive = false;
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────
bool Aimbot::HasTarget() {
    return s_target != nullptr && s_target->IsAlive();
}

void Aimbot::Run(int stage)
{
    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;

    // ── Stage 7: restore visual angles for silent aim ─────────────────────────
    if (stage == 7) {
        if (Globals::aimbot_enabled && Globals::aimbot_silent && s_savedValid) {
            __try {
                float* va = reinterpret_cast<float*>(client + AB_VIEW_ANGLES);
                va[0] = s_savedAngles[0]; va[1] = s_savedAngles[1]; va[2] = 0.f;
                auto* local = EntityManager::Get().GetLocalPawn();
                if (local && local->IsAlive())
                    WriteEyeAngles(local, { s_savedAngles[0], s_savedAngles[1], 0.f });
                s_savedValid = false;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
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

    if (!Globals::aimbot_enabled) {
        s_target = nullptr; s_savedValid = false; s_smoothValid = false;
        // Still run triggerbot
        __try {
            auto* local = EntityManager::Get().GetLocalPawn();
            if (local && local->IsAlive()) {
                Vector eye = local->m_vOldOrigin() + local->m_vecViewOffset();
                Vector view = ReadViewAngles(client);
                RunTriggerbot(local, eye, view);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return;
    }

    __try {
        auto* local = EntityManager::Get().GetLocalPawn();
        if (!local || !local->IsAlive()) {
            s_target = nullptr; s_savedValid = false; s_smoothValid = false; return;
        }

        // Key gate
        if (Globals::aimbot_key != 0) {
            if (!(GetAsyncKeyState(Globals::aimbot_key) & 0x8000)) {
                s_target = nullptr; s_savedValid = false; s_smoothValid = false; return;
            }
        }

        Vector eye = local->m_vOldOrigin() + local->m_vecViewOffset();
        Vector view = ReadViewAngles(client);

        // Run triggerbot in parallel
        RunTriggerbot(local, eye, view);

        // Save angles for silent aim (once when key first pressed)
        if (Globals::aimbot_silent && !s_savedValid) {
            s_savedAngles[0] = view.x;
            s_savedAngles[1] = view.y;
            s_savedAngles[2] = 0.f;
            s_savedValid = true;
        }

        // Validate locked target
        if (s_target && !s_target->IsAlive()) {
            s_target = nullptr;
            s_lastKillTime = now;
            s_hadTarget = true;
            s_smoothValid = false;
        }

        // Kill delay gate
        if (s_hadTarget && Globals::aimbot_kill_delay > 0.f) {
            long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - s_lastKillTime).count();
            if (ms < static_cast<long long>(Globals::aimbot_kill_delay * 1000.f)) {
                s_savedValid = false; s_smoothValid = false; return;
            }
            s_hadTarget = false;
        }

        // Find best target
        Vector targetPos;
        float  targetFov = 0.f;

        if (Globals::aimbot_lock_target && s_target) {
            if (!BestBoneForTarget(s_target, eye, view, Globals::aimbot_fov * 3.f,
                targetPos, targetFov))
            {
                s_target = nullptr; s_smoothValid = false;
            }
        }

        if (!s_target) {
            C_CSPlayerPawn* bestPawn = nullptr;
            float  bestFov = Globals::aimbot_fov;
            Vector bestPos; float bfv = 0.f;
            for (const auto& e : EntityManager::Get().GetEntities()) {
                if (!e.pawn || !e.pawn->IsAlive()) continue;
                if (!Globals::aimbot_friendly_fire && !e.isEnemy) continue;
                Vector bp; float fv;
                if (!BestBoneForTarget(e.pawn, eye, view, bestFov, bp, fv)) continue;
                bestFov = fv; bestPawn = e.pawn; bestPos = bp; bfv = fv;
            }
            if (bestPawn) {
                if (bestPawn != s_target) {
                    s_target = bestPawn;
                    s_targetSince = now;
                    s_smoothValid = false; // reset smooth on new target
                }
                targetPos = bestPos; targetFov = bfv;
            }
        }

        if (!s_target) { s_savedValid = false; s_smoothValid = false; return; }

        // Shot delay gate
        long long msAcquire = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - s_targetSince).count();
        if (msAcquire < static_cast<long long>(Globals::aimbot_shot_delay * 1000.f)) {
            s_savedValid = false; s_smoothValid = false; return;
        }

        // Target angle — NO recoil comp in smooth path (use RCS for that)
        Vector aimAng = Utils::CalcAngle(eye, targetPos);
        Utils::NormalizeAngles(aimAng);

        // For silent aim: snap directly
        if (Globals::aimbot_silent) {
            WriteViewAngles(client, aimAng);
            WriteEyeAngles(local, aimAng);
            return;
        }

        // ─ Time-based smooth interpolation ───────────────────────────────────
        if (!s_smoothValid) {
            s_smoothAngle = view;
            s_smoothValid = true;
        }
        else {
            // Detect external view-angle jump (recoil, punch) and re-sync.
            // If view moved more than 5° away from what we wrote last frame,
            // the game modified the angle on its own (recoil / punch).
            Vector chk = { view.x - s_smoothAngle.x, view.y - s_smoothAngle.y, 0.f };
            Utils::NormalizeAngles(chk);
            if (sqrtf(chk.x * chk.x + chk.y * chk.y) > 5.f)
                s_smoothAngle = view;
        }

        float smooth = std::max(1.f, Globals::aimbot_smooth);
        float speed = 360.f / smooth;  // deg/s
        float maxStep = speed * dt;

        Vector delta = { aimAng.x - s_smoothAngle.x, aimAng.y - s_smoothAngle.y, 0.f };
        Utils::NormalizeAngles(delta);
        float dist = sqrtf(delta.x * delta.x + delta.y * delta.y);

        if (dist < 0.01f) {
            s_smoothAngle = aimAng;
        }
        else if (dist > maxStep) {
            float t = maxStep / dist;
            s_smoothAngle.x += delta.x * t;
            s_smoothAngle.y += delta.y * t;
        }
        else {
            s_smoothAngle = aimAng;
        }
        Utils::NormalizeAngles(s_smoothAngle);

        WriteViewAngles(client, s_smoothAngle);
        WriteEyeAngles(local, s_smoothAngle);

        // Auto fire — only if smooth aim is very close to target
        if (Globals::aimbot_auto_fire) {
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

    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        s_target = nullptr; s_savedValid = false; s_smoothValid = false;
    }
}
