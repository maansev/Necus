#include "RageInternal.h"
#include "../../sdk/utils/Globals.h"
#include <Windows.h>
#include <chrono>

// ─── Auto fire ───────────────────────────────────────────────────────────────
// Rate-limited left click, fired after a valid steer angle is produced this
// frame so the bullet always coincides with the aim steering.
//
// Refine Shot governs how aggressively the fire timing is adjusted:
//   Off         — standard 60ms cooldown (~16 clicks/sec)
//   Latency     — minimal cooldown (8ms); fires at the earliest possible moment
//                 each game tick, maximising per-bullet accuracy at the cost of
//                 slightly higher CPU / input-spam rate
//   Performance — relaxed 120ms cooldown; reduces OS input overhead on low-fps
//                 or CPU-limited systems; may miss a tick now and then
namespace Rage {

void AutoFire()
{
    using clk = std::chrono::steady_clock;
    static clk::time_point s_last = clk::now();

    // Cooldown governed by Refine Shot mode
    long long limitMs;
    switch (Globals::rage_refine_shot) {
        case 1:  limitMs = 8;   break;   // Latency
        case 2:  limitMs = 120; break;   // Performance
        default: limitMs = 60;  break;   // Off
    }

    auto now = clk::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last).count();
    if (ms < limitMs) return;
    s_last = now;

    INPUT inp[2] = {};
    inp[0].type = INPUT_MOUSE; inp[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inp[1].type = INPUT_MOUSE; inp[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, inp, sizeof(INPUT));
}

} // namespace Rage
