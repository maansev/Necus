#include "Misc.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/entity/EntityManager.h"
#include "../../sdk/memory/Offsets.h"

void Misc::Render() {}

void Misc::RunBunnyHop(CUserCmd* cmd)
{
    if (!cmd || !Globals::misc_bhop)
        return;

    auto local = EntityManager::Get().GetLocalPawn();
    if (!local || !local->IsAlive())
        return;

    // Проверка на лестницу
    uint8_t moveType = *(uint8_t*)((uintptr_t)local + Offsets::m_MoveType);
    if (moveType == 9) // MOVETYPE_LADDER
        return;

    static bool lastJumped = false;
    static bool shouldJump = false;

    if (!lastJumped && shouldJump)
    {
        shouldJump = false;
        cmd->pButtons->m_nBits |= (1ULL << 1); // IN_JUMP
    }
    else if (cmd->pButtons->m_nBits & (1ULL << 1))
    {
        int flags = *(int*)((uintptr_t)local + Offsets::m_fFlags);
        if (flags & 1) // FL_ONGROUND
        {
            lastJumped = true;
            shouldJump = true;
        }
        else
        {
            cmd->pButtons->m_nBits &= ~(1ULL << 1);
            lastJumped = false;
        }
    }
    else
    {
        lastJumped = false;
        shouldJump = false;
    }
}