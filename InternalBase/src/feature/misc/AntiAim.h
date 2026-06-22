#pragma once

// Anti-Aim: fakes the networked view angle so enemies can't hit your real
// hitbox orientation. Applied to CCSGOInput frame history in the CreateMove
// hook (same mechanism as silent aim) — the local camera is never modified.
namespace AntiAim {
    // Returns true if anti-aim should be applied this tick (enabled, keyed,
    // alive, valid move type, not firing). When true, writes the fake pitch/yaw
    // into out_pitch / out_yaw. realPitch/realYaw are the player's true angles.
    bool Compute(float realPitch, float realYaw, float& out_pitch, float& out_yaw);
}
