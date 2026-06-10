#include "Viewmodel.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/Cvar.h"
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
//  Viewmodel editor.
//
//  In CS2 the schema fields m_flViewmodelOffsetX/Y/Z/FOV on the pawn are driven
//  from the player's convars and get overwritten by the game every frame, so
//  writing them directly does nothing. The working mechanism is to set the
//  convars themselves (viewmodel_offset_x/y/z, viewmodel_fov) via ICvar, with
//  FCVAR_CHEAT cleared so they apply in normal games.
// ─────────────────────────────────────────────────────────────────────────────

static bool  s_active = false;

void Viewmodel::Update()
{
    if (Globals::viewmodel_enabled) {
        Cvar::SetFloat("viewmodel_offset_x", Globals::viewmodel_x);
        Cvar::SetFloat("viewmodel_offset_y", Globals::viewmodel_y);
        Cvar::SetFloat("viewmodel_offset_z", Globals::viewmodel_z);
        Cvar::SetFloat("viewmodel_fov", Globals::viewmodel_fov);
        s_active = true;
    }
    else if (s_active) {
        // restore CS2 stock viewmodel
        Cvar::SetFloat("viewmodel_offset_x", 1.0f);
        Cvar::SetFloat("viewmodel_offset_y", 2.0f);
        Cvar::SetFloat("viewmodel_offset_z", -2.0f);
        Cvar::SetFloat("viewmodel_fov", 68.0f);
        s_active = false;
    }
}
