#pragma once
#include <cstdint>
#include "../memory/Offsets.h"
#include "../utils/Vector.h"

#define SCHEMA(type, name, offset) \
    type name() const { \
        return *reinterpret_cast<const type*>(reinterpret_cast<uintptr_t>(this) + offset); \
    }

struct CInButtonState {
    uint64_t m_nBits;
    uint64_t m_nHeld;
    uint64_t m_nReleased;
};

class CUserCmd {
public:
    void* vmt;
    char pad_0008[24];
    CInButtonState* pButtons;
};

class CGameSceneNode {
public:
    SCHEMA(uintptr_t, m_modelState, Offsets::m_modelState);
};

class C_Player_ObserverServices {
public:
    SCHEMA(uint32_t, m_hObserverTarget, Offsets::m_hObserverTarget);
};

class C_BaseEntity {
public:
    SCHEMA(int, m_iHealth, Offsets::m_iHealth);
    SCHEMA(int, m_iTeamNum, Offsets::m_iTeamNum);
    SCHEMA(Vector, m_vOldOrigin, Offsets::m_vOldOrigin);
    SCHEMA(uintptr_t, m_pGameSceneNode, Offsets::m_pGameSceneNode);
    bool IsAlive() const { return m_iHealth() > 0; }
};

class C_CSPlayerPawn : public C_BaseEntity {
public:
    SCHEMA(Vector, m_vecViewOffset, Offsets::m_vecViewOffset);
    SCHEMA(int, m_iShotsFired, Offsets::m_iShotsFired);
    SCHEMA(Vector, m_aimPunchAngle, Offsets::m_aimPunchAngle);
    SCHEMA(uintptr_t, m_pObserverServices, Offsets::m_pObserverServices);
};

class C_CSPlayerController : public C_BaseEntity {
public:
    SCHEMA(uint32_t, m_hPlayerPawn, Offsets::m_hPlayerPawn);
    SCHEMA(const char*, m_iszPlayerName, Offsets::m_iszPlayerName);
    SCHEMA(bool, m_bPawnIsAlive, Offsets::m_bPawnIsAlive);
};

// ==================== ÄËß ÑÊÈÍ×ÅÉÍÄÆÅÐÀ ====================
struct CUtlVectorSimple {
    void* m_elements;
    int     m_size;
    int     m_capacity;
};

struct CHandleSimple {
    uint32_t m_handle;
};

class CBaseAnimGraph : public C_BaseEntity {};

class C_EconItemView {
public:
    uint16_t& m_iItemDefinitionIndex() { return *(uint16_t*)((uintptr_t)this + Offsets::m_iItemDefinitionIndex); }
    int& m_iEntityQuality() { return *(int*)((uintptr_t)this + 0x1BC); }
    char* m_szCustomName() { return (char*)((uintptr_t)this + 0x2F8); }

    uint16_t m_iItemDefinitionIndexC() const { return *(const uint16_t*)((uintptr_t)this + Offsets::m_iItemDefinitionIndex); }
};

class C_AttributeContainer {
public:
    C_EconItemView* GetItemPtr() { return (C_EconItemView*)((uintptr_t)this + 0x50); }
};

class C_EconEntity : public CBaseAnimGraph {
public:
    int& m_nFallbackPaintKit() { return *(int*)((uintptr_t)this + 0x1658); }
    int& m_nFallbackSeed() { return *(int*)((uintptr_t)this + 0x165C); }
    float& m_flFallbackWear() { return *(float*)((uintptr_t)this + 0x1660); }
    int& m_nFallbackStatTrak() { return *(int*)((uintptr_t)this + 0x1664); }

    C_AttributeContainer* GetAttrContainer() { return (C_AttributeContainer*)((uintptr_t)this + Offsets::m_AttributeManager); }
};

class C_BasePlayerWeapon : public C_EconEntity {
public:
    SCHEMA(int, m_iClip1, 0x16D8);
    SCHEMA(int, m_iClip2, 0x16DC);
};