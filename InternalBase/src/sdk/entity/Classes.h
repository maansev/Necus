#pragma once
#include <cstdint>
#include "../memory/Offsets.h"
#include "../memory/PatternScan.h"
#include "../utils/Vector.h"

#define SCHEMA(type, name, offset) \
    type& name() { return *reinterpret_cast<type*>(reinterpret_cast<uintptr_t>(this) + offset); }
#define SCHEMA_C(type, name, offset) \
    type name() const { return *reinterpret_cast<const type*>(reinterpret_cast<uintptr_t>(this) + offset); }

// ─── Raw VMT call ─────────────────────────────────────────────────────────────
template<typename Ret = void, typename... Args>
static Ret vmt_call(void* obj, int idx, Args... args) {
    auto** vmt = *reinterpret_cast<void***>(obj);
    using Fn = Ret(__fastcall*)(void*, Args...);
    return reinterpret_cast<Fn>(vmt[idx])(obj, args...);
}

// ─── Forward structs ─────────────────────────────────────────────────────────
struct CInButtonState { uint64_t m_nBits, m_nHeld, m_nReleased; };

class CUserCmd {
public:
    void* vmt;
    char pad_0008[24];
    CInButtonState* pButtons;
};

class CGameSceneNode {
public:
    SCHEMA(uintptr_t, m_modelState, Offsets::m_modelState);
    SCHEMA(uintptr_t, m_child, 0x68);   // CGameSceneNode* pChild
    SCHEMA(uintptr_t, m_nextSibling, 0x70);   // CGameSceneNode* pNextSibling
    SCHEMA(uintptr_t, m_owner, 0x50);   // CEntityInstance* pOwner

    void set_mesh_group_mask(uint64_t mask) {
        using fn_t = void(__fastcall*)(void*, uint64_t);
        static auto fn = reinterpret_cast<fn_t>(
            Memory::PatternScan("client.dll",
                "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8D 99 ? ? ? ? 48 8B 71")
            );
        if (fn) fn(this, mask);
    }
};

class C_Player_ObserverServices {
public:
    SCHEMA(uint32_t, m_hObserverTarget, Offsets::m_hObserverTarget);
};

class C_BaseEntity {
public:
    SCHEMA_C(int, m_iHealth, Offsets::m_iHealth);
    SCHEMA_C(int, m_iTeamNum, Offsets::m_iTeamNum);
    SCHEMA(Vector, m_vOldOrigin, Offsets::m_vOldOrigin);
    SCHEMA(uintptr_t, m_pGameSceneNode, Offsets::m_pGameSceneNode);
    SCHEMA(uint32_t, m_nSubclassID, 0x2BC);   // from cs2-dumper C_BaseEntity::m_nSubclassID
    bool IsAlive() const { return m_iHealth() > 0; }
};

class C_CSPlayerPawn : public C_BaseEntity {
public:
    SCHEMA(Vector, m_vecViewOffset, Offsets::m_vecViewOffset);
    SCHEMA(int, m_iShotsFired, Offsets::m_iShotsFired);
    SCHEMA(Vector, m_aimPunchAngle, Offsets::m_aimPunchAngle);
    SCHEMA(uintptr_t, m_pObserverServices, Offsets::m_pObserverServices);
    SCHEMA(uintptr_t, m_pWeaponServices, Offsets::m_pWeaponServices);
};

class C_CSPlayerController : public C_BaseEntity {
public:
    SCHEMA(uint32_t, m_hPlayerPawn, Offsets::m_hPlayerPawn);
    SCHEMA_C(const char*, m_iszPlayerName, Offsets::m_iszPlayerName);
    SCHEMA(bool, m_bPawnIsAlive, Offsets::m_bPawnIsAlive);
};

// ─── Skin / econ types ────────────────────────────────────────────────────────
struct CUtlVectorSimple {
    void* m_elements;
    int   m_size;
};

struct CHandleSimple { uint32_t m_handle; };

// Attribute list vector layout in CS2 (confirmed from working project)
struct ptr_game_vector_t {
    uint64_t  size;   // element count
    uintptr_t ptr;    // pointer to array
};

class C_AttributeList {
public:
    // m_Attributes (C_UtlVectorEmbeddedNetworkVar) at +0x8
    ptr_game_vector_t& m_attributes() {
        return *reinterpret_cast<ptr_game_vector_t*>(reinterpret_cast<uintptr_t>(this) + 0x8);
    }
};

class C_EconItemView {
public:
    // Fields confirmed by cs2-dumper 2026-06-03
    SCHEMA(uint16_t, m_iItemDefinitionIndex, Offsets::m_iItemDefinitionIndex);   // 0x1BA
    SCHEMA(int32_t, m_iEntityQuality, 0x1BC);
    SCHEMA(uint64_t, m_iItemID, 0x1C8);
    SCHEMA(uint32_t, m_iItemIDHigh, 0x1D0);  // set to 0xFFFFFFFF to force fallback
    SCHEMA(uint32_t, m_iItemIDLow, 0x1D4);
    SCHEMA(uint32_t, m_iAccountID, 0x1D8);
    SCHEMA(bool, m_bInitialized, 0x1E8);
    SCHEMA(bool, m_bDisallowSOC, 0x1E9);
    SCHEMA(bool, m_bRestoreCustomMaterialAfterPrecache, 0x1B8);
    SCHEMA_C(uint16_t, m_iItemDefinitionIndexC, Offsets::m_iItemDefinitionIndex);

    char* m_szCustomName() { return reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(this) + 0x2F8); }

    // m_AttributeList (CAttributeList inline) at +0x208
    C_AttributeList* m_attribute_list() {
        return reinterpret_cast<C_AttributeList*>(reinterpret_cast<uintptr_t>(this) + 0x208);
    }
};

class C_AttributeContainer {
public:
    // m_Item (C_EconItemView inline) at +0x50  (confirmed C_AttributeContainer::m_Item = 0x50)
    C_EconItemView* GetItemPtr() {
        return reinterpret_cast<C_EconItemView*>(reinterpret_cast<uintptr_t>(this) + 0x50);
    }
};

class CBaseAnimGraph : public C_BaseEntity {};

class C_EconEntity : public CBaseAnimGraph {
public:
    // Fallback fields (cs2-dumper C_BasePlayerWeapon / C_EconEntity confirmed)
    SCHEMA(int, m_nFallbackPaintKit, 0x1658);   // m_paint_kit
    SCHEMA(int, m_nFallbackSeed, 0x165C);   // m_seed
    SCHEMA(float, m_flFallbackWear, 0x1660);   // m_wear
    SCHEMA(int, m_nFallbackStatTrak, 0x1664);

    // m_AttributeManager (C_AttributeContainer inline) at 0x1180
    C_AttributeContainer* GetAttrContainer() {
        return reinterpret_cast<C_AttributeContainer*>(
            reinterpret_cast<uintptr_t>(this) + Offsets::m_AttributeManager);
    }

    // VMT calls (indices confirmed from working project c_cs_player_pawn.hpp)
    void update_composite(bool force = true) { vmt_call<void>(this, 10, force); }
    void update_skin(bool force = true) { vmt_call<void>(this, 110, force); }
    void update_weapon_data() { vmt_call<void*>(this, 195); }

    CGameSceneNode* m_scene_node() {
        return reinterpret_cast<CGameSceneNode*>(m_pGameSceneNode());
    }

    void set_model(const char* path) {
        using fn_t = void(__fastcall*)(void*, const char*);
        static auto fn = reinterpret_cast<fn_t>(
            Memory::PatternScan("client.dll",
                "40 53 48 83 EC ? 48 8B D9 4C 8B C2 48 8B 0D ? ? ? ? 48 8D 54 24 40")
            );
        if (fn) fn(this, path);
    }

    void update_subclass(uint16_t def_index) {
        // Compute Murmur hash of def_index as string → write to m_nSubclassID
        char buf[16];
        sprintf_s(buf, "%u", (unsigned)def_index);
        uint32_t hash = 0x31415926;
        const unsigned char* d = reinterpret_cast<const unsigned char*>(buf);
        int len = (int)strlen(buf);
        uint32_t m = 0x5bd1e995;
        hash ^= (uint32_t)len;
        while (len >= 4) { uint32_t k = *(uint32_t*)d; k *= m; k ^= k >> 24; k *= m; hash *= m; hash ^= k; d += 4; len -= 4; }
        switch (len) { case 3:hash ^= d[2] << 16; [[fallthrough]]; case 2:hash ^= d[1] << 8; [[fallthrough]]; case 1:hash ^= d[0]; hash *= m; }
                             hash ^= hash >> 13; hash *= m; hash ^= hash >> 15;
                             m_nSubclassID() = hash;

                             using fn_t = void(__fastcall*)(void*);
                             static auto fn = reinterpret_cast<fn_t>(
                                 Memory::PatternScan("client.dll", "4C 8B DC 53 48 81 EC ?? ?? ?? ?? 48 8B 41")
                                 );
                             if (fn) fn(this);
    }
};

class C_BasePlayerWeapon : public C_EconEntity {};
