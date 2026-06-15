#pragma once
#include <cstdint>
#include <Windows.h>

// CS2 mesh draw color (4 RGBA bytes inside CMeshData)
struct DrawColor { uint8_t r, g, b, a; };

// CS2 material_data_t — DrawObject 3rd argument (scenesystem.dll hook):
//   +0x14  pMaterial    (CMaterial2*, overwrite to swap shader)
//   +0x40  color        (RGBA vertex tint, 4 bytes)
//   +0x44  pObjectInfo  (object_info_t*; +176 = e_id: player_t=104, player_ct=113)
struct CMeshData {
    char      _pad_00[0x14];   // +0x00
    void*     pMaterial;       // +0x14
    char      _pad_1C[0x24];   // +0x1C
    DrawColor color;           // +0x40
    void*     pObjectInfo;     // +0x44
};

// Player type IDs at *(int*)((uintptr_t)pObjectInfo + 176)
static constexpr int kObjectId_PlayerT  = 104;
static constexpr int kObjectId_PlayerCT = 113;

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

    // The actual hook function installed over DrawArray in scenesystem.dll.
    void __fastcall HookDrawArray(void* a1, void* a2, CMeshData* meshData, int nCount,
                                  void* sceneView, void* sceneLayer, void* u1, void* u2);
}
