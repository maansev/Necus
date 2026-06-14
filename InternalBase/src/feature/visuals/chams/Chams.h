#pragma once
#include <cstdint>
#include <Windows.h>

// CS2 mesh draw color (4 RGBA bytes inside CMeshData)
struct DrawColor { uint8_t r, g, b, a; };

// CS2 CMeshData — DrawObject argument, offsets confirmed (Aspasia research 2024-2025):
//   +0x18  pSceneAnimatableObject  (CAnimatableSceneObject*)
//   +0x20  pMaterial               (CMaterial2*, overwrite to swap shader)
//   +0x40  color                   (RGBA vertex tint, 4 bytes)
struct CMeshData {
    char      _pad_00[0x18];          // +0x00
    void*     pSceneAnimatableObject; // +0x18
    void*     pMaterial;              // +0x20
    char      _pad_28[0x18];          // +0x28
    DrawColor color;                  // +0x40
};

// Material visual styles (must match order materials are created in Init)
enum class ChamsStyle : int {
    FLAT       = 0,  // csgo_unlitgeneric — solid flat colour
    ILLUMINATE = 1,  // csgo_complex + F_SELF_ILLUM — glowing/lit
    GLOW_XQZ   = 2,  // csgo_effects + fresnel — rim-glow (XQZ variant)
    COUNT
};

namespace Chams {
    // Initialise: pattern-scan KV3/CreateMaterial, build all materials.
    // Returns true if materials were successfully created.
    // Call once after DX11 device is available (from hkPresent first-init).
    bool Init();

    // Tear down material handles.
    void Shutdown();

    // Set when Init() completes so the hook knows materials are ready.
    inline bool g_materialsReady = false;

    // oDrawArray — stored by Hooks.cpp when the DrawArray hook is installed.
    using DrawArrayFn = void(__fastcall*)(void*, void*, CMeshData*, int, void*, void*, void*, void*);
    inline DrawArrayFn oDrawArray = nullptr;

    // Lock-free entity snapshot — written once per frame by Present thread
    // (call UpdateSnapshot after EntityManager::Update), read by HookDrawArray
    // on the same render thread. Eliminates per-draw-call mutex + GetModuleHandleA.
    struct SnapEntry { uint32_t pawnHandle; bool isEnemy; };
    inline SnapEntry g_snap[64];
    inline int       g_snapCount = 0;

    // Called from hkPresent after EntityManager::Get().Update().
    void UpdateSnapshot();

    // The actual hook function installed over DrawArray in scenesystem.dll.
    void __fastcall HookDrawArray(void* a1, void* a2, CMeshData* meshData, int nCount,
                                  void* sceneView, void* sceneLayer, void* u1, void* u2);
}
