#include "Cvar.h"
#include <Windows.h>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
//  CS2 ConVar accessor implementation.
//
//  Layout notes (Source 2, 2024-2026 builds):
//    - ICvar comes from CreateInterface("VEngineCvar007") in tier0.dll.
//    - FindConVar(name, bAllowDeveloper) is at ICvar vtable index 14 and
//      returns a 16-bit ConVarHandle (packed in a 64-bit register).
//    - GetConVar(handle) at vtable index 11 returns a ConVarData*.
//    - ConVarData has the FCVAR flags at +0x30 (uint32) and the current value
//      union (CVValue_t) at +0x40. For a float convar the float lives at +0x40.
//
//  These indices/offsets are the verification points if a build changes them.
// ─────────────────────────────────────────────────────────────────────────────

namespace {
    using FindConVarFn = uint16_t(__thiscall*)(void* thisptr, const char* name, bool allowDeveloper);
    using GetConVarFn  = void*  (__thiscall*)(void* thisptr, uint16_t handle);

    void*  g_cvar      = nullptr;
    bool   g_ready     = false;

    constexpr int kVT_FindConVar = 14;
    constexpr int kVT_GetConVar  = 11;

    constexpr uintptr_t kConVar_Flags = 0x30;
    constexpr uintptr_t kConVar_Value = 0x40;

    constexpr uint32_t FCVAR_CHEAT = (1u << 14);

    template <typename T>
    T VFunc(void* inst, int idx) {
        void** vt = *reinterpret_cast<void***>(inst);
        return reinterpret_cast<T>(vt[idx]);
    }
}

bool Cvar::Init()
{
    if (g_ready) return true;
    __try {
        HMODULE tier0 = GetModuleHandleA("tier0.dll");
        if (!tier0) return false;
        using CreateInterfaceFn = void*(*)(const char*, int*);
        auto ci = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(tier0, "CreateInterface"));
        if (!ci) return false;
        g_cvar = ci("VEngineCvar007", nullptr);
        g_ready = (g_cvar != nullptr);
        return g_ready;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool Cvar::Ready() { return g_ready; }

void Cvar::SetFloat(const char* name, float value)
{
    if (!g_ready && !Init()) return;
    __try {
        auto find = VFunc<FindConVarFn>(g_cvar, kVT_FindConVar);
        uint16_t handle = find(g_cvar, name, false);
        if (handle == 0xFFFF) return;

        auto get = VFunc<GetConVarFn>(g_cvar, kVT_GetConVar);
        void* data = get(g_cvar, handle);
        if (!data) return;

        uintptr_t d = reinterpret_cast<uintptr_t>(data);
        // strip FCVAR_CHEAT so the convar is settable without sv_cheats
        uint32_t* flags = reinterpret_cast<uint32_t*>(d + kConVar_Flags);
        *flags &= ~FCVAR_CHEAT;

        // write the current float value
        *reinterpret_cast<float*>(d + kConVar_Value) = value;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
