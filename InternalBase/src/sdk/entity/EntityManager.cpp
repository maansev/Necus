#include "EntityManager.h"
#include "../memory/PatternScan.h"
#include "../memory/Offsets.h"
#include <Windows.h>
#include <chrono>
#include <iostream>

// SEH helpers live in plain functions (no C++ objects with destructors),
// otherwise MSVC raises C2712 when mixing __try with unwinding.
namespace {
    static bool SafeReadPtr(uintptr_t addr, uintptr_t& out) {
        if (!addr) return false;
        __try { out = *reinterpret_cast<uintptr_t*>(addr); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    static bool SafeReadU32(uintptr_t addr, uint32_t& out) {
        if (!addr) return false;
        __try { out = *reinterpret_cast<uint32_t*>(addr); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    static bool SafeReadI32(uintptr_t addr, int32_t& out) {
        if (!addr) return false;
        __try { out = *reinterpret_cast<int32_t*>(addr); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    static bool SafeReadU8(uintptr_t addr, uint8_t& out) {
        if (!addr) return false;
        __try { out = *reinterpret_cast<uint8_t*>(addr); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // Resolve entity pointer from handle via the entity list. SEH-safe, no C++ objects.
    static uintptr_t ResolveFromList(uintptr_t listBase, uint32_t handle) {
        uintptr_t entry = 0;
        if (!SafeReadPtr(listBase + 8 * ((handle & 0x7FFF) >> 9) + 16, entry) || !entry)
            return 0;
        uintptr_t ptr = 0;
        if (!SafeReadPtr(entry + 112 * (handle & 0x1FF), ptr))
            return 0;
        return ptr;
    }
}

EntityManager::EntityManager()
{
    // entityListAddress is refreshed every Update(); do not cache here.
}

EntityManager& EntityManager::Get()
{
    static EntityManager instance;
    return instance;
}

void EntityManager::Update()
{
    // Always re-derive from current module base so re-injects / relocated modules work.
    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;

    entityListAddress = client + Offsets::dwEntityList;

    uintptr_t listBase = 0;
    if (!SafeReadPtr(entityListAddress, listBase) || !listBase) return;

    C_CSPlayerPawn* currentLocalPawn = nullptr;
    {
        uintptr_t localPawnAddr = 0;
        if (SafeReadPtr(client + Offsets::dwLocalPlayerPawn, localPawnAddr) && localPawnAddr)
            currentLocalPawn = reinterpret_cast<C_CSPlayerPawn*>(localPawnAddr);
    }

    int localTeam = 0;
    if (currentLocalPawn) {
        uint8_t tb = 0;
        if (SafeReadU8(reinterpret_cast<uintptr_t>(currentLocalPawn) + Offsets::m_iTeamNum, tb))
            localTeam = tb;
    }

    std::vector<Entity_t> temp;
    temp.reserve(64);

    for (int i = 1; i < 64; ++i)
    {
        uintptr_t controllerPtr = ResolveFromList(listBase, static_cast<uint32_t>(i));
        if (!controllerPtr)
            continue;

        uint32_t pawnHandle = 0;
        if (!SafeReadU32(controllerPtr + Offsets::m_hPlayerPawn, pawnHandle) ||
            !pawnHandle || pawnHandle == static_cast<uint32_t>(-1))
            continue;

        uintptr_t pawnPtr = ResolveFromList(listBase, pawnHandle);
        if (!pawnPtr)
            continue;

        auto pawn = reinterpret_cast<C_CSPlayerPawn*>(pawnPtr);
        if (pawn == currentLocalPawn)
            continue;

        int32_t health = 0;
        if (!SafeReadI32(pawnPtr + Offsets::m_iHealth, health) || health <= 0)
            continue;

        uint8_t teamNum = 0;
        if (!SafeReadU8(pawnPtr + Offsets::m_iTeamNum, teamNum))
            continue;

        Entity_t ent{};
        ent.controller  = reinterpret_cast<C_CSPlayerController*>(controllerPtr);
        ent.pawn        = pawn;
        ent.index       = i;
        ent.isEnemy     = !currentLocalPawn || (static_cast<int>(teamNum) != localTeam);
        ent.pawnHandle  = pawnHandle;

        temp.push_back(ent);
    }

    {
        std::unique_lock lock(mutex);
        entities.swap(temp);
        localPawn = currentLocalPawn;
    }
}

C_CSPlayerPawn* EntityManager::GetPawnFromHandle(uint32_t handle)
{
    if (!handle) return nullptr;

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return nullptr;

    uintptr_t listBase = 0;
    if (!SafeReadPtr(client + Offsets::dwEntityList, listBase) || !listBase)
        return nullptr;

    return reinterpret_cast<C_CSPlayerPawn*>(ResolveFromList(listBase, handle));
}

uintptr_t EntityManager::GetEntityFromHandle(uint32_t handle)
{
    if (!handle) return 0;

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return 0;

    uintptr_t listBase = 0;
    if (!SafeReadPtr(client + Offsets::dwEntityList, listBase) || !listBase)
        return 0;

    return ResolveFromList(listBase, handle);
}

C_CSPlayerPawn* EntityManager::GetLocalPawn()
{
    std::shared_lock lock(mutex);
    return localPawn;
}

const std::vector<Entity_t>& EntityManager::GetEntities()
{
    std::shared_lock lock(mutex);
    return entities;
}
