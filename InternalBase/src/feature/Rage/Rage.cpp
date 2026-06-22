#include "Rage.h"
#include "RageInternal.h"
#include "../combat/legitbot/Aimbot.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/utils/Utils.h"
#include "../../menu/Menu.h"
#include <Windows.h>

// ─── Rage orchestrator ───────────────────────────────────────────────────────
// One call per FSN stage-0 frame. Resolves the active weapon/profile, picks a
// target, steers the bullet through Aimbot::g_silent* (the shared frame-history
// channel) and optionally auto-fires. Keeps zero persistent state of its own —
// every submodule it calls owns whatever it needs.
namespace Rage {

// True when any UI is up (cheat menu or a game panel that shows the OS cursor) —
// firing then would spam clicks into that UI. Plain function, no objects → __try
// would be legal, but we don't need it here.
static bool IsAnyMenuOpen()
{
    if (Menu::IsOpen) return true;
    CURSORINFO ci = { sizeof(CURSORINFO) };
    if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING)) return true;
    return false;
}

void Run(C_CSPlayerPawn* local, const Vector& eye, const Vector& view)
{
    // Default: nothing to inject this frame. The CreateMove hook clears the bullet
    // steer when g_silentValid is false.
    Aimbot::g_silentValid = false;

    if (!Globals::rage_enabled) return;
    if (Globals::rage_key != 0 && !(GetAsyncKeyState(Globals::rage_key) & 0x8000)) return;
    if (IsAnyMenuOpen()) return;

    Context ctx;
    ctx.local     = local;
    ctx.eye       = eye;
    ctx.view      = view;
    ctx.weaponDef = ActiveWeaponDef(local);
    if (ctx.weaponDef == 0 || IsExcludedWeapon(ctx.weaponDef)) return;
    ctx.slot = ProfileSlot(ctx.weaponDef);

    // Manual vs automatic firing:
    //   Automatic Fire ON  → steer + fire as soon as a target is locked.
    //   Automatic Fire OFF → behave like fire-assist: only steer while LMB held.
    bool autoFire = Globals::aimbot_auto_fire;
    if (!autoFire && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;

    Vector point;
    C_CSPlayerPawn* tgt = SelectTarget(ctx, point);
    if (!tgt) return;

    // Steer the bullet at the chosen point via the shared silent channel. The
    // camera is never touched — only the networked angle in the frame history.
    Vector ang = Utils::CalcAngle(eye, point);
    Utils::NormalizeAngles(ang);
    Aimbot::g_silentPitch = ang.x;
    Aimbot::g_silentYaw   = ang.y;
    Aimbot::g_silentValid = true;

    // Don't machine-gun the mouse when the magazine is empty. clip == 0 only when
    // we read a plausible value; -1 (unknown / stale offset) still fires so the
    // gate can never silently disable shooting.
    if (autoFire && ActiveWeaponClip(local) != 0) AutoFire();
}

} // namespace Rage
