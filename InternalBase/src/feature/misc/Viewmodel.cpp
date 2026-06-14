#include "Viewmodel.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/Offsets.h"
#include "../../sdk/entity/EntityManager.h"
#include <Windows.h>
#include <cmath>

static bool  s_saved = false;
static float s_origX = 0.f, s_origY = 0.f, s_origZ = 0.f, s_origFov = 68.f;

void Viewmodel::Update(int stage)
{
    auto* pawn = EntityManager::Get().GetLocalPawn();
    if (!pawn || !pawn->IsAlive()) {
        s_saved = false;
        return;
    }

    uintptr_t base = (uintptr_t)pawn;

    __try {
        float* x = (float*)(base + Offsets::m_flViewmodelOffsetX);
        float* y = (float*)(base + Offsets::m_flViewmodelOffsetY);
        float* z = (float*)(base + Offsets::m_flViewmodelOffsetZ);
        float* fov = (float*)(base + Offsets::m_flViewmodelFOV);

        if (Globals::viewmodel_enabled) {
            if (!s_saved) {
                s_origX = *x;
                s_origY = *y;
                s_origZ = *z;
                s_origFov = *fov;
                s_saved = true;
            }

            // Пишем на stage 0 и stage 5 — самый надёжный способ
            if (stage == 0 || stage == 5) {
                *x = Globals::viewmodel_x;
                *y = Globals::viewmodel_y;
                *z = Globals::viewmodel_z;
                *fov = Globals::viewmodel_fov;
            }
        }
        else if (s_saved) {
            *x = s_origX;
            *y = s_origY;
            *z = s_origZ;
            *fov = s_origFov;
            s_saved = false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}