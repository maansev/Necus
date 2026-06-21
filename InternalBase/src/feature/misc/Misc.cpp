#include "Misc.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/memory/Offsets.h"

void Misc::Render() {}

// ─── Auto Accept ──────────────────────────────────────────────────────────────
// Lobby-only: scans the centre of the game window for the green "Accept"
// button and clicks it. Guarded by "no local pawn" so it can never misfire
// on green pixels in an actual match.
void Misc::RunAutoAccept(HWND gameWindow)
{
    if (!Globals::misc_auto_accept || !gameWindow)
        return;

    static DWORD s_next = 0;
    DWORD now = GetTickCount();
    if (now < s_next) return;
    s_next = now + 250;

    if (EntityManager::Get().GetLocalPawn()) return;      // already in a match
    if (GetForegroundWindow() != gameWindow) return;       // game not focused

    RECT rc{};
    if (!GetClientRect(gameWindow, &rc)) return;
    const int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
    if (cw < 100 || ch < 100) return;

    HDC screen = GetDC(nullptr);
    if (!screen) return;

    // The accept button sits horizontally centred, roughly mid-screen.
    const float ys[] = { 0.44f, 0.48f, 0.52f, 0.56f, 0.60f };
    for (float fy : ys) {
        POINT pt{ cw / 2, static_cast<LONG>(ch * fy) };
        ClientToScreen(gameWindow, &pt);
        COLORREF c = GetPixel(screen, pt.x, pt.y);
        int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
        // green accept button: strongly green-dominant pixel
        if (g > 110 && g > r * 2 && g > b * 2) {
            POINT old{}; GetCursorPos(&old);
            SetCursorPos(pt.x, pt.y);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            SetCursorPos(old.x, old.y);
            s_next = now + 3000;   // cooldown after a click
            break;
        }
    }
    ReleaseDC(nullptr, screen);
}

// Classic "perfect bhop":
//   if (holdingJump) { if (onGround) press jump; else release jump; }
// Releasing in air lets the engine re-queue the jump on landing — smoother
// than the previous double-flag state machine.
void Misc::RunBunnyHop(CUserCmd* cmd)
{
    if (!cmd || !cmd->pButtons || !Globals::misc_bhop)
        return;

    auto local = EntityManager::Get().GetLocalPawn();
    if (!local || !local->IsAlive())
        return;

    // Skip on ladders / noclip
    uint8_t moveType = *(uint8_t*)((uintptr_t)local + Offsets::m_MoveType);
    if (moveType == 9 /*MOVETYPE_LADDER*/ || moveType == 8 /*MOVETYPE_NOCLIP*/)
        return;

    constexpr uint64_t IN_JUMP = (1ULL << 1);
    bool holdingJump = (cmd->pButtons->m_nBits & IN_JUMP) != 0;
    if (!holdingJump)
        return;   // user not holding jump — never touch the cmd

    int  flags = *(int*)((uintptr_t)local + Offsets::m_fFlags);
    bool onGround = (flags & 1) != 0;   // FL_ONGROUND

    if (onGround)
        cmd->pButtons->m_nBits |= IN_JUMP;     // press on ground
    else
        cmd->pButtons->m_nBits &= ~IN_JUMP;    // release in air
}