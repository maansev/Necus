#include "RageInternal.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/utils/Utils.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/memory/Offsets.h"
#include <Windows.h>
#include <cstdlib>
#include <cmath>

// ─── Target + aim-point selection ────────────────────────────────────────────
namespace Rage {

// Rage hitbox slots aligned to the menu labels { Head, Chest, Stomach, Arms,
// Legs, Feet } — note this is a DIFFERENT mapping than the legit aimbot's
// kBoneMap, on purpose, so the rage UI and the bones it scans match 1:1.
static const BoneID kRageBones[6] = {
    BoneID::Head,          // 0 Head
    BoneID::Chest,         // 1 Chest
    BoneID::Pelvis,        // 2 Stomach
    BoneID::RightShoulder, // 3 Arms
    BoneID::RightKnee,     // 4 Legs
    BoneID::RightFoot,     // 5 Feet
};
// Multipoint labels { Head, Chest, Stomach } → the same three core bones.
static const BoneID kRageMultipoint[3] = {
    BoneID::Head, BoneID::Chest, BoneID::Pelvis,
};

// Plain helper (no C++ objects in scope) so __try is legal here.
static bool IsSpotted(C_CSPlayerPawn* pawn) {
    __try {
        return *reinterpret_cast<bool*>(
            reinterpret_cast<uintptr_t>(pawn) + Offsets::m_entitySpottedState + 0x8);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Plain helper (no C++ objects in scope) so __try is legal here.
static int SafeHealth(C_CSPlayerPawn* pawn) {
    __try { return pawn->m_iHealth(); } __except (EXCEPTION_EXECUTE_HANDLER) { return 100; }
}

static inline float AngleFov(const Vector& a, const Vector& b) {
    Vector d = { a.x - b.x, a.y - b.y, 0.f };
    Utils::NormalizeAngles(d);
    return std::sqrt(d.x * d.x + d.y * d.y);
}

// Best aim point on a single pawn within `fovLimit`. Scans the union of the
// profile's enabled hitboxes and multipoint bones; returns the closest-to-view
// point that also clears the profile's minimum damage.
static bool BestPointOn(const Context& ctx, C_CSPlayerPawn* pawn,
                        float fovLimit, Vector& outPoint, float& outFov)
{
    const auto& rp = Globals::g_rageProfiles[ctx.slot];
    float best  = fovLimit;
    bool  found = false;

    // Min-damage gate. mindmg == 0 means "Auto": just require a real hit (≥1) and
    // let the bone scan pick the highest-damage point. Otherwise we never demand
    // more than the target's current HP — a guaranteed kill is enough, asking for
    // more only throws away valid lethal shots (matches the "Force Lethal" idea).
    int hp = SafeHealth(pawn);
    int wantDmg = rp.mindmg;
    if (wantDmg <= 0)      wantDmg = 1;        // Auto
    else if (wantDmg > hp) wantDmg = hp;       // cap to lethal

    auto consider = [&](BoneID bone) {
        Vector bp = Utils::GetBonePos(pawn, bone);
        if (bp.IsZero()) return;
        Vector ang = Utils::CalcAngle(ctx.eye, bp);
        Utils::NormalizeAngles(ang);
        float fov = AngleFov(ang, ctx.view);
        if (fov >= best) return;
        if (EstimateDamage(ctx, pawn, bp) < (float)wantDmg) return;
        best = fov; outPoint = bp; outFov = fov; found = true;
    };

    for (int i = 0; i < 6; ++i) if (rp.hitboxes[i])   consider(kRageBones[i]);
    for (int i = 0; i < 3; ++i) if (rp.multipoint[i]) consider(kRageMultipoint[i]);
    return found;
}

// Candidate enemy + its chosen aim point, used to apply the target-mode tiebreak
// across everyone that passed the fov / damage gates.
struct Candidate {
    C_CSPlayerPawn* pawn;
    Vector          point;
    float           fov;
};

// Hit-chance gate: maps the 0-100 slider to a maximum LOCAL player speed.
// hitchance=100 → must be nearly still (≤12 u/s); hitchance=0 → no gate.
// extrapolation adds a flat speed allowance to relax the gate.
// force_shot bypasses the gate entirely.
static bool PassesHitChance(const Context& ctx, const Globals::RageWeaponProfile& rp)
{
    if (rp.force_shot || rp.hitchance <= 0) return true;
    float maxSpeed = (100.f - (float)rp.hitchance) * 2.5f + 12.f + rp.extrapolation;
    float* vel = nullptr;
    __try {
        vel = reinterpret_cast<float*>(
            reinterpret_cast<uintptr_t>(ctx.local) + Offsets::m_vecVelocity);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return true; }
    if (!vel) return true;
    float speed = std::sqrt(vel[0]*vel[0] + vel[1]*vel[1]);
    return speed <= maxSpeed;
}

C_CSPlayerPawn* SelectTarget(const Context& ctx, Vector& outPoint)
{
    const auto& rp = Globals::g_rageProfiles[ctx.slot];

    // Hit-chance gate applies to ALL targets — no point scanning the entity list
    // if our own movement makes a hit impossible (unless Force Shot is on).
    if (!PassesHitChance(ctx, rp)) return nullptr;

    Candidate best{};
    bool haveBest = false;
    float bestScore = 0.f;   // meaning depends on rp.target

    for (const auto& e : EntityManager::Get().GetEntities()) {
        if (!e.pawn || !e.pawn->IsAlive()) continue;
        if (!Globals::aimbot_friendly_fire && !e.isEnemy) continue;

        // Visibility gate: when Aim Through Walls is off, only shoot spotted enemies.
        if (!Globals::aimbot_auto_wall && !IsSpotted(e.pawn)) continue;

        Vector pt; float fov;
        if (!BestPointOn(ctx, e.pawn, Globals::rage_fov, pt, fov)) continue;

        // Score per target mode; higher score wins.
        // Modes: 0=HighestDamage 1=HighestHitChance 2=Closest 3=LowestHP 4=Random
        float score;
        switch (rp.target) {
            case 1: { // Highest Hit Chance — player must be most still (smallest fov and lowest speed)
                // Approximate hit chance as inverse FOV angle (smaller = easier shot).
                // EstimateDamage already gates min damage; pick the point the bot
                // is most likely to physically hit this tick.
                score = 100.f - fov;
                break;
            }
            case 2: { // Closest to crosshair → smallest FOV angle wins
                score = -fov;
                break;
            }
            case 3: { // Lowest HP → finish off wounded targets first
                score = -(float)SafeHealth(e.pawn);
                break;
            }
            case 4: { // Random
                score = (float)(std::rand() % 1000);
                break;
            }
            case 0:
            default: { // Highest damage (default)
                score = EstimateDamage(ctx, e.pawn, pt);
                break;
            }
        }

        if (!haveBest || score > bestScore) {
            haveBest = true; bestScore = score;
            best = { e.pawn, pt, fov };
        }
    }

    if (!haveBest) return nullptr;
    outPoint = best.point;
    return best.pawn;
}

} // namespace Rage
