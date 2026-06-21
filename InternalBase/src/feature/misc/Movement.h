#pragma once

namespace Movement {
    void RunAutostrafer();    // computes oscillation + presses A/D button (called from CreateMove)
    void RunInfiniteDuck();
    void RunQuickStop();

    // Written by RunAutostrafer, read by Hooks::hkPresent to write va[1] at render frequency.
    // Present runs at 200+ FPS and reliably overrides SDL mouse input.
    extern bool  g_strafeActive;
    extern float g_strafeTargetYaw;
}
