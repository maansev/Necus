#pragma once
#include "../../sdk/entity/Classes.h"
#include "../../sdk/utils/Vector.h"

// ─── Rage ─────────────────────────────────────────────────────────────────────
// Standalone "rage" combat mode, kept fully separate from the legit Aimbot so the
// two never share state or fight over the same view-angle writes. Rage steers the
// bullet through the same frame-history channel the legit silent aim uses
// (Aimbot::g_silent*), so the CCSGOInput::CreateMove hook needs no changes.
//
// Entry point: Rage::Run() is called once per FSN stage-0 frame from Aimbot::Run
// while Globals::rage_enabled is true. Everything else lives in the rage/ folder:
//   RageProfile.cpp   — weapon resolution + per-weapon profile selection
//   RageTarget.cpp    — enemy + aim-point selection (fov / hitboxes / multipoint)
//   RageAutoWall.cpp  — estimated damage gate (distance falloff + armor)
//   RageAutoFire.cpp  — automatic trigger
namespace Rage {
    void Run(C_CSPlayerPawn* local, const Vector& eye, const Vector& view);
}
