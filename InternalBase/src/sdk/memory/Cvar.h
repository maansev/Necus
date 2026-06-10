#pragma once
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
//  Minimal CS2 (Source 2) ConVar accessor.
//
//  CS2 keeps the viewmodel position/FOV under the player's convars
//  (viewmodel_offset_x/y/z, viewmodel_fov). The schema fields on the pawn are
//  driven from these, so writing the convar is the mechanism that actually
//  moves the viewmodel. Several of these convars carry FCVAR_CHEAT; we clear
//  that flag so they can be changed in normal (non-sv_cheats) games.
//
//  Interface: "VEngineCvar007" exported via CreateInterface from tier0.dll.
//  If a future build bumps the version, change kCvarInterface below.
// ─────────────────────────────────────────────────────────────────────────────

namespace Cvar
{
    // Resolve the ICvar interface once. Returns false if unavailable.
    bool Init();

    // Set a float convar by name (clears FCVAR_CHEAT first). No-op on failure.
    void SetFloat(const char* name, float value);

    // True once Init() has found the interface.
    bool Ready();
}
