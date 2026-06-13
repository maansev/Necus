#include "Viewmodel.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/Offsets.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/entity/Classes.h"
#include <cstdint>
#include <cmath>
#include <Windows.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Viewmodel editor — pawn schema fields (safe, no freeze).
//
//  NOTE: the ICvar path froze the game (unverified vtable layout), so it is
//  removed. This writes the pawn fields directly; it will not crash, but the
//  game may overwrite the values during view calc on some builds. Kept safe
//  until a verified viewmodel mechanism is available.
// ─────────────────────────────────────────────────────────────────────────────

static bool  s_saved = false;
static float s_origX = 0.f, s_origY = 0.f, s_origZ = 0.f, s_origFov = 68.f;
static int   s_forceFrames = 0;   // force-write window right after enabling
static bool  s_wasEnabled = false;

void Viewmodel::Update(int stage)
{
    auto* pawn = EntityManager::Get().GetLocalPawn();
    if (!pawn || !pawn->IsAlive()) { s_saved = false; return; }

    s_wasEnabled = Globals::viewmodel_enabled;

    uintptr_t base = reinterpret_cast<uintptr_t>(pawn);

    __try {
        float* x = reinterpret_cast<float*>(base + Offsets::m_flViewmodelOffsetX);
        float* y = reinterpret_cast<float*>(base + Offsets::m_flViewmodelOffsetY);
        float* z = reinterpret_cast<float*>(base + Offsets::m_flViewmodelOffsetZ);
        float* fov = reinterpret_cast<float*>(base + Offsets::m_flViewmodelFOV);

        if (Globals::viewmodel_enabled) {
            if (!s_saved) {
                s_origX = *x; s_origY = *y; s_origZ = *z; s_origFov = *fov;
                s_saved = true;
            }
            // Stage 5 (FRAME_RENDER_START): always force-write so the engine
            // reads our values for the current frame render.
            // Stage 0 (prediction): eps guard avoids redundant writes.
            if (stage == 5) {
                *x = Globals::viewmodel_x;
                *y = Globals::viewmodel_y;
                *z = Globals::viewmodel_z;
                *fov = Globals::viewmodel_fov;
            }
            else {
                const float eps = 0.005f;
                if (fabsf(*x - Globals::viewmodel_x) > eps) *x = Globals::viewmodel_x;
                if (fabsf(*y - Globals::viewmodel_y) > eps) *y = Globals::viewmodel_y;
                if (fabsf(*z - Globals::viewmodel_z) > eps) *z = Globals::viewmodel_z;
                if (fabsf(*fov - Globals::viewmodel_fov) > eps) *fov = Globals::viewmodel_fov;
            }
        }
        else if (s_saved) {
            *x = s_origX; *y = s_origY; *z = s_origZ; *fov = s_origFov;
            s_saved = false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
