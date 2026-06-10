#include "Chams.h"
#include "../../../sdk/utils/Globals.h"
#include "../../../sdk/entity/EntityManager.h"
#include "../../../sdk/entity/Classes.h"
#include <d3d11.h>
#include <windows.h>

// Указатель на оригинальную функцию
using DrawArrayFn = void(__fastcall*)(void*, void*, void*, int, void*, void*, void*, void*);
DrawArrayFn oDrawArray = nullptr;

// Материалы
static void* g_pMaterialFlat = nullptr;
static void* g_pMaterialIlluminate = nullptr;
static void* g_pMaterialGlow = nullptr;

// Простейший интерфейс ресурсной системы
class IResourceSystem {
public:
    virtual void* CreateMaterial(const char* name, int flags) = 0;
};

static IResourceSystem* GetResourceSystem()
{
    using CreateInterfaceFn = void* (*)(const char*, int*);

    // В CS2 ResourceSystem живёт в resourcesystem.dll
    HMODULE rs = GetModuleHandleA("resourcesystem.dll");
    if (!rs) rs = GetModuleHandleA("engine2.dll");
    if (!rs) return nullptr;

    auto CreateInterface = (CreateInterfaceFn)GetProcAddress(rs, "CreateInterface");
    if (!CreateInterface) return nullptr;

    // Перебираем актуальные версии CS2
    for (int i = 20; i >= 1; i--) {
        char name[32];
        sprintf_s(name, "VResourceSystem%03d", i);
        auto* res = (IResourceSystem*)CreateInterface(name, nullptr);
        if (res) return res;
    }
    return nullptr;
}

bool chams::Init()
{
    auto pRes = GetResourceSystem();
    if (!pRes) return false;

    // Загружаем готовые материалы, которые точно есть в игре
    g_pMaterialFlat = pRes->CreateMaterial("materials/dev/primary_white.vmat", 0);
    g_pMaterialIlluminate = pRes->CreateMaterial("materials/dev/illuminate.vmat", 0);
    g_pMaterialGlow = pRes->CreateMaterial("materials/dev/glow.vmat", 0);

    if (!g_pMaterialIlluminate) g_pMaterialIlluminate = g_pMaterialFlat;
    if (!g_pMaterialGlow) g_pMaterialGlow = g_pMaterialFlat;
    return true;
}

void chams::Shutdown()
{
    // Игра сама управляет материалами
}

static void* GetMaterialByIndex(int index)
{
    switch (index) {
    case 0: return g_pMaterialFlat;
    case 1: return g_pMaterialIlluminate;
    case 2: return g_pMaterialGlow;
    default: return g_pMaterialFlat;
    }
}

void __fastcall chams::DrawArrayHook(void* a1, void* a2, void* pMeshScene, int nMeshCount,
    void* pSceneView, void* pSceneLayer, void* pUnk, void* pUnk2)
{
    if (!oDrawArray) return;

    // Получаем данные меша
    auto pMesh = reinterpret_cast<uintptr_t*>(pMeshScene);
    if (!pMesh)
    {
        oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
        return;
    }

    // pSceneAnimatableObject лежит по смещению 0x8
    uintptr_t pAnimatable = pMesh[1];
    if (!pAnimatable)
    {
        oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
        return;
    }

    // m_hOwner находится по смещению 0x28 внутри CGameSceneNode
    uintptr_t hOwner = *(uintptr_t*)(pAnimatable + 0x28);
    if (!hOwner)
    {
        oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
        return;
    }

    // Получаем указатель на entity через наш EntityManager
    uintptr_t entityPtr = EntityManager::Get().GetEntityFromHandle((uint32_t)(hOwner & 0x7FFF));
    if (!entityPtr)
    {
        oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
        return;
    }

    auto entity = reinterpret_cast<C_CSPlayerPawn*>(entityPtr);
    if (!entity || !entity->IsAlive())
    {
        oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
        return;
    }

    auto local = EntityManager::Get().GetLocalPawn();
    if (!local || entity == local)
    {
        oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
        return;
    }

    bool isEnemy = (entity->m_iTeamNum() != local->m_iTeamNum());
    const auto& visCol = isEnemy ? Globals::enemy_chams_visible_color : Globals::team_chams_visible_color;
    const auto& invCol = isEnemy ? Globals::enemy_chams_invisible_color : Globals::team_chams_invisible_color;
    int matIdx = isEnemy ? Globals::enemy_chams_material : Globals::team_chams_material;
    bool enabled = isEnemy ? Globals::enemy_chams_enabled : Globals::team_chams_enabled;

    if (!enabled)
    {
        oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
        return;
    }

    // Invisible pass (XQZ)
    if (invCol[3] > 0.01f)
    {
        auto* mat = GetMaterialByIndex(matIdx);
        *(void**)((uintptr_t)pMeshScene + 0x10) = mat;   // pMaterial
        auto* color = (float*)((uintptr_t)pMeshScene + 0x24); // цвет
        color[0] = invCol[0]; color[1] = invCol[1];
        color[2] = invCol[2]; color[3] = invCol[3];
        oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
    }

    // Visible pass
    if (visCol[3] > 0.01f)
    {
        auto* mat = GetMaterialByIndex(matIdx);
        *(void**)((uintptr_t)pMeshScene + 0x10) = mat;
        auto* color = (float*)((uintptr_t)pMeshScene + 0x24);
        color[0] = visCol[0]; color[1] = visCol[1];
        color[2] = visCol[2]; color[3] = visCol[3];
        oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
        return;
    }

    oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
}