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

// ─── Internal offsets ────────────────────────────────────────────────────────
static constexpr ptrdiff_t AB_VIEW_ANGLES = 0x23558B8;

// ─── State ───────────────────────────────────────────────────────────────────
static C_CSPlayerPawn* s_target = nullptr;
static float  s_savedAngles[3] = {};
static bool   s_savedValid = false;
static auto   s_targetSince = std::chrono::steady_clock::now();
static auto   s_lastAutoFire = std::chrono::steady_clock::now();
static auto   s_lastKillTime = std::chrono::steady_clock::now();
static bool   s_hadTarget = false; // track kill for kill_delay

// ─── Hitbox → BoneID mapping ─────────────────────────────────────────────────
// 0=Head  1=Neck  2=Chest  3=Stomach  4=Arms  5=Legs
static const BoneID kBoneMap[6] = {
    BoneID::Head,
    BoneID::Neck,
    BoneID::Spine,
    BoneID::Pelvis,
    BoneID::LeftShoulder,
    BoneID::LeftHip,
};

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

// Scan all selected hitboxes, return the one closest to crosshair.
static bool BestBoneForTarget(C_CSPlayerPawn* pawn, const Vector& eye,
    const Vector& view, float fovLimit,
    Vector& outPos, float& outFov)
{
    float best = fovLimit;
    bool  found = false;
    for (int i = 0; i < 6; ++i) {
        if (!Globals::aimbot_hitboxes[i]) continue;
        Vector bp = Utils::GetBonePos(pawn, kBoneMap[i]);
        if (bp.IsZero()) continue;
        Vector aim = Utils::CalcAngle(eye, bp);
        Utils::NormalizeAngles(aim);
        float fov = AngleFov(aim, view);
        if (fov < best) { best = fov; outPos = bp; outFov = fov; found = true; }
    }
    return found;
}

// ─── Public API ──────────────────────────────────────────────────────────────
bool Aimbot::HasTarget() {
    return s_target != nullptr && s_target->IsAlive();
}

void Aimbot::Run(int stage)
{
    if (!Globals::aimbot_enabled) {
        s_target = nullptr;
        s_savedValid = false;
        return;
    }

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;

    // ── Stage 0 : aim calculation ─────────────────────────────────────────────
    if (stage == 0) {
        __try {
            auto* local = EntityManager::Get().GetLocalPawn();
            if (!local || !local->IsAlive()) {
                s_target = nullptr; s_savedValid = false; return;
            }

            if (Globals::aimbot_key != 0) {
                bool keyHeld = (GetAsyncKeyState(Globals::aimbot_key) & 0x8000) != 0;
                if (!keyHeld) { s_target = nullptr; s_savedValid = false; return; }
            }

            Vector eye = local->m_vOldOrigin() + local->m_vecViewOffset();
            Vector view = ReadViewAngles(client);

            // Save original angles for silent restore (once per key press)
            if (Globals::aimbot_silent && !s_savedValid) {
                s_savedAngles[0] = view.x;
                s_savedAngles[1] = view.y;
                s_savedAngles[2] = 0.f;
                s_savedValid = true;
            }

            // Validate locked target — record kill time when target dies
            if (s_target && !s_target->IsAlive()) {
                s_target = nullptr;
                s_lastKillTime = std::chrono::steady_clock::now();
                s_hadTarget = true;
            }

            // ─ Kill delay gate ────────────────────────────────────────────────
            auto now0 = std::chrono::steady_clock::now();
            if (s_hadTarget && Globals::aimbot_kill_delay > 0.f) {
                long long msKill = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now0 - s_lastKillTime).count();
                if (msKill < static_cast<long long>(Globals::aimbot_kill_delay * 1000.f)) {
                    s_savedValid = false; return;
                }
                else {
                    s_hadTarget = false;
                }
            }

            // ─ Find best target ───────────────────────────────────────────────
            Vector targetPos;
            float  targetFov = 0.f;

            if (Globals::aimbot_lock_target && s_target) {
                // Try to keep current target even slightly outside FOV
                if (!BestBoneForTarget(s_target, eye, view,
                    Globals::aimbot_fov * 3.f,
                    targetPos, targetFov))
                    s_target = nullptr;
            }

            if (!s_target) {
                C_CSPlayerPawn* bestPawn = nullptr;
                float  bestFov = Globals::aimbot_fov;
                Vector bestPos;
                float  bfv = 0.f;

                const auto& ents = EntityManager::Get().GetEntities();
                for (const auto& e : ents) {
                    if (!e.pawn || !e.pawn->IsAlive()) continue;
                    if (!Globals::aimbot_friendly_fire && !e.isEnemy) continue;
                    Vector bp; float fv;
                    if (!BestBoneForTarget(e.pawn, eye, view, bestFov, bp, fv)) continue;
                    bestFov = fv; bestPawn = e.pawn; bestPos = bp; bfv = fv;
                }

                if (bestPawn) {
                    s_target = bestPawn;
                    s_targetSince = std::chrono::steady_clock::now();
                    targetPos = bestPos;
                    targetFov = bfv;
                }
            }

            if (!s_target) { s_savedValid = false; return; }

            // ─ Shot delay gate (blocks aiming until reaction time elapsed) ────
            auto now1 = std::chrono::steady_clock::now();
            long long msSinceAcquire = std::chrono::duration_cast<std::chrono::milliseconds>(
                now1 - s_targetSince).count();
            if (msSinceAcquire < static_cast<long long>(Globals::aimbot_shot_delay * 1000.f)) {
                s_savedValid = false; return; // target found but reaction delay not elapsed
            }

            // ─ Aim angle ─────────────────────────────────────────────────────
            Vector aimAng = Utils::CalcAngle(eye, targetPos);

            // Recoil compensation: CS2 uses aimPunchAngle * 2
            if (!Globals::aimbot_silent) {
                Vector punch = local->m_aimPunchAngle();
                aimAng.x -= punch.x * 2.f;
                aimAng.y -= punch.y * 2.f;
            }
            Utils::NormalizeAngles(aimAng);

            // ─ Smooth ────────────────────────────────────────────────────────
            Vector newAng;
            if (Globals::aimbot_silent) {
                newAng = aimAng;
            }
            else {
                float smooth = std::max(1.f, Globals::aimbot_smooth);
                Vector d = { aimAng.x - view.x, aimAng.y - view.y, 0.f };
                Utils::NormalizeAngles(d);
                newAng.x = view.x + d.x / smooth;
                newAng.y = view.y + d.y / smooth;
                newAng.z = 0.f;
                Utils::NormalizeAngles(newAng);
            }

            // ─ Write angles ───────────────────────────────────────────────────
            WriteViewAngles(client, newAng);
            if (!Globals::aimbot_silent) WriteEyeAngles(local, newAng);

            // ─ Auto fire ──────────────────────────────────────────────────────
            if (Globals::aimbot_auto_fire) {
                float onTargetFov = AngleFov(newAng, aimAng);
                if (onTargetFov < 0.8f) {
                    auto now = std::chrono::steady_clock::now();
                    long long msSinceFire = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - s_lastAutoFire).count();
                    if (msSinceFire >= 60) {
                        // Simulate left click through SendInput (reliable, no offset needed)
                        INPUT inp[2] = {};
                        inp[0].type = INPUT_MOUSE; inp[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                        inp[1].type = INPUT_MOUSE; inp[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                        SendInput(2, inp, sizeof(INPUT));
                        s_lastAutoFire = now;
                    }
                }
            }

        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            s_target = nullptr; s_savedValid = false;
        }
    }

    // ── Stage 7 : restore visual angles for silent aim ────────────────────────
    if (stage == 7 && Globals::aimbot_silent && s_savedValid) {
        __try {
            float* va = reinterpret_cast<float*>(client + AB_VIEW_ANGLES);
            va[0] = s_savedAngles[0];
            va[1] = s_savedAngles[1];
            va[2] = 0.f;

            auto* local = EntityManager::Get().GetLocalPawn();
            if (local && local->IsAlive()) WriteEyeAngles(local, { s_savedAngles[0], s_savedAngles[1], 0.f });
            s_savedValid = false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}
