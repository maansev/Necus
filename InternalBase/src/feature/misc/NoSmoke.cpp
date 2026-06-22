#include "NoSmoke.h"
#include "../../sdk/utils/Globals.h"
#include "../../sdk/memory/Offsets.h"
#include <cstdint>
#include <climits>
#include <cstring>
#include <Windows.h>

static uintptr_t SafeRead(uintptr_t addr)
{
    __try { return *reinterpret_cast<uintptr_t*>(addr); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// stride 112 — original value that was finding smoke entities correctly
static uintptr_t GetEnt(uintptr_t listPtr, int i)
{
    __try {
        uintptr_t entry = SafeRead(listPtr + (8 * ((i & 0x7FFF) >> 9)) + 16);
        if (!entry) return 0;
        return SafeRead(entry + 112 * (i & 0x1FF));
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// class name check via ent+0x10 → schema → +0x20 → char*
// confirmed working in the original build (was finding smoke grenades correctly)
static bool IsSmokeGrenade(uintptr_t ent)
{
    __try {
        uintptr_t schema = SafeRead(ent + 0x10);
        if (schema < 0x10000u) return false;
        uintptr_t namePtr = SafeRead(schema + 0x20);
        if (namePtr < 0x10000u) return false;
        const char* name = reinterpret_cast<const char*>(namePtr);
        return strncmp(name, "smokegrenade_projectile", 23) == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void NoSmoke::Update(uintptr_t listPtr)
{
    if (!Globals::misc_nosmoke || !listPtr) return;

    for (int i = 64; i < 2048; ++i)
    {
        uintptr_t ent = GetEnt(listPtr, i);
        if (!ent) continue;
        if (!IsSmokeGrenade(ent)) continue;

        __try {
            // m_nSmokeEffectTickBegin = 0x1250 (cs2-dumper 2026-06-03)
            // INT_MAX: game's "currentTick >= tickBegin" check never passes → no render
            *reinterpret_cast<int*>(ent + Offsets::m_nSmokeEffectTickBegin) = INT_MAX;
            *reinterpret_cast<bool*>(ent + Offsets::m_bDidSmokeEffect) = false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}
