#pragma once

class Aimbot {
public:
    static void Run(int stage);
    static bool HasTarget();
    static void RunRCS_Stage5();  // called from FSN5 (punch valid after prediction)

    // Silent aim: aimbot sets these instead of writing dwViewAngles.
    // The CCSGOInput::CreateMove hook reads and clears them each frame.
    static inline float g_silentPitch = 0.f;
    static inline float g_silentYaw   = 0.f;
    static inline bool  g_silentValid = false;

    // RCS: punch cached at FSN5 (after prediction) for use in CreateMove hook.
    // Base + velocity×fraction gives the full interpolated punch angle.
    static inline float g_rcsPunchPitch = 0.f;
    static inline float g_rcsPunchYaw   = 0.f;
    static inline int   g_rcsShotsFired = 0;
    static inline bool  g_rcsEnabled    = false;
};
