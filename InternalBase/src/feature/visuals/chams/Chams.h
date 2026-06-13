#pragma once
#include <cstdint>
#include <Windows.h>

// CS2 mesh draw color (4 RGBA bytes inside CMeshData)
struct DrawColor { uint8_t r, g, b, a; };

// CS2 mesh draw call descriptor — the engine passes an array of these to DrawArray.
// Layout confirmed from CS2 2024/2025 scene system research:
//   +0x00  pSceneAnimatableObject  (CAnimatableSceneObject*)
//   +0x18  pMaterial               (CMaterial2*, overwrite to swap shader)
//   +0x28  color                   (RGBA vertex tint)
struct CMeshData {
    void* pSceneAnimatableObject; // +0x00
    char      _pad_08[16];            // +0x08
    void* pMaterial;              // +0x18
    char      _pad_20[8];             // +0x20
    DrawColor color;                  // +0x28
};

// Material visual styles (must match order materials are created in Init)
enum class ChamsStyle : int {
    FLAT = 0,  // csgo_unlitgeneric — solid flat colour
    ILLUMINATE = 1,  // csgo_complex + F_SELF_ILLUM — glowing/lit
    GLOW_XQZ = 2,  // csgo_effects + fresnel — rim-glow (XQZ variant)
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

    // The actual hook function installed over DrawArray in scenesystem.dll.
    void __fastcall HookDrawArray(void* a1, void* a2, CMeshData* meshData, int nCount,
        void* sceneView, void* sceneLayer, void* u1, void* u2);
}
