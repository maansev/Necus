#pragma once

namespace Visuals
{
    void Render();
    void RunNightMode();
    void OnRenderStart();  // FrameStageNotify stage 5 — removals memory writes
    void OnRenderEnd();    // stage 7 — restore m_bIsScoped etc.
    void UpdateRemovals(); // call every hkPresent frame

    // Damage log (poll-based, like the hit marker): newest-first snapshot.
    struct DmgEntry { char name[48]; int dmg; int hp; double time; };
    int GetDamageLog(DmgEntry* out, int maxCount);
}
