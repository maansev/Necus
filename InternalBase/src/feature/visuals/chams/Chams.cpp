#include "Chams.h"
#include "../../../sdk/utils/Globals.h"
#include "../../../sdk/memory/Offsets.h"
#include "../../../sdk/memory/PatternScan.h"
#include "../../../sdk/entity/EntityManager.h"
#include "../../../sdk/entity/Classes.h"
#include "../../../sdk/utils/Log.h"
#include <Windows.h>
#include <algorithm>

// =============================================================================
//  CS2 Chams — VMaterialSystem2 approach (proven working source).
//
//  Material creation pipeline (matches the reference implementation):
//    1. matSys->find_material(&prototype, "materials/dev/primary_white.vmat")
//    2. matSys->set_create_data_by_material(buffer+0x50, &prototype)
//    3. set_material_shader_type(buffer+0x50, key("shader",0x162C1777), "<vfx>", 0x18)
//    4. set_material_functions(buffer+0x50, key("F_...",0), value, 0x12)   (repeat)
//    5. matSys->create_material(&out, name, buffer+0x50)  -> material_t*
//
//  Functions live in particles.dll; the material system is VMaterialSystem2_001
//  (materialsystem2.dll, via CreateInterface). vfunc indices: 14/29/37.
// =============================================================================

// ─── key_string_t (16 bytes, passed by value per game ABI) ────────────────────
struct KeyString {
    uintptr_t   key;
    const char* str;
};

// particles.dll function pointers
using SetShaderTypeFn = void      (__fastcall*)(void* data, KeyString shader, const char* shaderName, int);
using SetFunctionsFn  = void      (__fastcall*)(void* data, KeyString func,   int value, int);
using FindKeyFn       = uintptr_t (__fastcall*)(const char* name, int, int hash);

// VMaterialSystem2 vfuncs
using FindMaterialFn  = void* (__fastcall*)(void* thisptr, void** outMat, const char* name);
using SetCreateDataFn = void  (__fastcall*)(void* thisptr, void** material, const void* data);
using CreateMaterial2Fn = void* (__fastcall*)(void* thisptr, void** outMat, const char* name,
                                              const void* data, int, int, int, int, int);

static SetShaderTypeFn   s_SetShaderType = nullptr;
static SetFunctionsFn    s_SetFunctions  = nullptr;
static FindKeyFn         s_FindKey       = nullptr;
static void*             s_matSys        = nullptr;

static constexpr uintptr_t kShaderKey   = 0x162C1777u;  // hash of "shader"
static constexpr int       kUnkKeyHash  = 0x31415926;

// Material slots: for each ChamsStyle, a visible layer and an XQZ (through-wall) layer.
static void* s_matVisible[(int)ChamsStyle::COUNT] = {};
static void* s_matXqz    [(int)ChamsStyle::COUNT] = {};

// ─── Style → shader + functions table ─────────────────────────────────────────
struct StyleDef {
    const char* shader;
    const char* funcs[4];   // null-terminated list of F_* functions to enable
};

// Visible variants (Z-tested). Invisible variants add a Z-disable function.
static const StyleDef kStyleVis[(int)ChamsStyle::COUNT] = {
    { "csgo_unlitgeneric.vfx", { "F_BLEND_MODE", "F_TRANSLUCENT", nullptr, nullptr } }, // FLAT
    { "csgo_unlitgeneric.vfx", { "F_BLEND_MODE", "F_TRANSLUCENT", nullptr, nullptr } }, // ILLUMINATE
    { "csgo_unlitgeneric.vfx", { "F_BLEND_MODE", "F_TRANSLUCENT", nullptr, nullptr } }, // GLOW
};
static const StyleDef kStyleXqz[(int)ChamsStyle::COUNT] = {
    { "csgo_unlitgeneric.vfx", { "F_BLEND_MODE", "F_TRANSLUCENT", "F_DISABLE_Z_BUFFERING", nullptr } },
    { "csgo_unlitgeneric.vfx", { "F_BLEND_MODE", "F_TRANSLUCENT", "F_DISABLE_Z_BUFFERING", nullptr } },
    { "csgo_unlitgeneric.vfx", { "F_BLEND_MODE", "F_TRANSLUCENT", "F_DISABLE_Z_BUFFERING", nullptr } },
};

// ─── CreateInterface helper ───────────────────────────────────────────────────
static void* GetInterface(const char* dll, const char* name) {
    HMODULE mod = GetModuleHandleA(dll);
    if (!mod) return nullptr;
    using CreateInterfaceFn = void* (__cdecl*)(const char* name, int* code);
    auto ci = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(mod, "CreateInterface"));
    if (!ci) return nullptr;
    __try { return ci(name, nullptr); } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

// ─── Material creation ────────────────────────────────────────────────────────
static void* CreateChamsMaterial(const char* name, const StyleDef& def) {
    if (!s_matSys || !s_SetShaderType || !s_SetFunctions) return nullptr;

    __try {
        // 0x1000-byte scratch buffer; the game's working data sits at +0x50.
        static thread_local uint8_t buffer[0x1000];
        memset(buffer, 0, sizeof(buffer));
        void* data = buffer + 0x50;

        void** vt = *reinterpret_cast<void***>(s_matSys);
        auto findMaterial = reinterpret_cast<FindMaterialFn>   (vt[14]);
        auto setData      = reinterpret_cast<SetCreateDataFn>  (vt[37]);
        auto createMat    = reinterpret_cast<CreateMaterial2Fn>(vt[29]);

        void* prototype = nullptr;
        findMaterial(s_matSys, &prototype, "materials/dev/primary_white.vmat");
        setData(s_matSys, &prototype, data);

        // Shader type
        KeyString shaderKey{ kShaderKey, "shader" };
        s_SetShaderType(data, shaderKey, def.shader, 0x18);

        // Functions (flags)
        for (int i = 0; i < 4 && def.funcs[i]; ++i) {
            uintptr_t k = s_FindKey ? s_FindKey(def.funcs[i], 0x12, kUnkKeyHash) : 0;
            KeyString fk{ k, def.funcs[i] };
            s_SetFunctions(data, fk, 1, 0x12);
        }

        void* out = nullptr;
        createMat(s_matSys, &out, name, data, 0, 0, 0, 0, 1);
        return out;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Write("Chams: CreateChamsMaterial(%s) faulted", name);
        return nullptr;
    }
}

// ─── Material name read (vtable[0] = get_name) ────────────────────────────────
static const char* GetMaterialName(void* mat) {
    if (!mat) return nullptr;
    __try {
        using GetNameFn = const char*(__fastcall*)(void*);
        auto vtbl = *reinterpret_cast<void***>(mat);
        return reinterpret_cast<GetNameFn>(vtbl[0])(mat);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

// Detect player by material name. Multiple prefixes for cross-build safety.
static bool IsPlayerMaterial(void* mat) {
    const char* name = GetMaterialName(mat);
    if (!name) return false;
    __try {
        return strstr(name, "characters/models") != nullptr
            || strstr(name, "models/player")     != nullptr
            || strstr(name, "/player/")          != nullptr
            || strstr(name, "player/custom")     != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

namespace { int s_dbgCalls = 0; int s_dbgPlayerHits = 0; }

// ─── Apply tint colour ────────────────────────────────────────────────────────
static inline void ApplyColor(CMeshData* m, const float* col4) {
    m->color.r = (uint8_t)std::clamp(col4[0] * 255.f, 0.f, 255.f);
    m->color.g = (uint8_t)std::clamp(col4[1] * 255.f, 0.f, 255.f);
    m->color.b = (uint8_t)std::clamp(col4[2] * 255.f, 0.f, 255.f);
    m->color.a = (uint8_t)std::clamp(col4[3] * 255.f, 0.f, 255.f);
}

// ─── Chams::Init ─────────────────────────────────────────────────────────────
bool Chams::Init() {
    // Material system interface (materialsystem2.dll → VMaterialSystem2_001)
    s_matSys = GetInterface("materialsystem2.dll", "VMaterialSystem2_001");
    Log::Write("Chams::Init VMaterialSystem2=%p", s_matSys);

    // set_material_shader_type — particles.dll, resolved from a relative call.
    {
        uintptr_t callSite = Memory::PatternScan("particles.dll",
            "E8 ? ? ? ? 48 8D B7 ? ? ? ?");
        Log::Write("Chams::Init SetShaderType callsite=0x%llX", (unsigned long long)callSite);
        if (callSite) {
            int32_t rel = *reinterpret_cast<int32_t*>(callSite + 1);
            uintptr_t fn = callSite + 5 + static_cast<uintptr_t>(static_cast<intptr_t>(rel));
            s_SetShaderType = reinterpret_cast<SetShaderTypeFn>(fn);
            Log::Write("Chams::Init SetShaderType resolved=0x%llX", (unsigned long long)fn);
        }
    }

    // set_material_functions — particles.dll
    {
        uintptr_t fn = Memory::PatternScan("particles.dll",
            "48 89 5C 24 08 48 89 6C 24 10 56 57 41 54");
        Log::Write("Chams::Init SetFunctions=0x%llX", (unsigned long long)fn);
        if (fn) s_SetFunctions = reinterpret_cast<SetFunctionsFn>(fn);
    }

    // find_key (find_var_material) — particles.dll
    {
        uintptr_t fn = Memory::PatternScan("particles.dll",
            "48 89 5C 24 08 57 48 81 EC C0 00 00 00 33 C0 8B");
        Log::Write("Chams::Init FindKey=0x%llX", (unsigned long long)fn);
        if (fn) s_FindKey = reinterpret_cast<FindKeyFn>(fn);
    }

    if (!s_matSys || !s_SetShaderType || !s_SetFunctions) {
        Log::Write("Chams::Init material API not found — color-only mode");
        g_materialsReady = false;
        return false;
    }

    // ── Material creation DISABLED ────────────────────────────────────────────
    // CreateChamsMaterial() faults inside particles.dll (wrong KeyString ABI).
    // Catching an AV *inside* a system DLL with SEH unwinds our stack but leaves
    // particles.dll's internal heap-lock / critical section held -> a few seconds
    // later another thread AV-crashes in ntdll heap code (ntdll+0x736E4, read of
    // 0xFFFF...FF). So we must NOT call into that code path until the ABI is
    // fixed. Run in color-only mode (no custom materials) — fully stable.
    Log::Write("Chams::Init material creation DISABLED (ABI unsafe) — color-only mode");
    for (int i = 0; i < (int)ChamsStyle::COUNT; ++i) {
        s_matVisible[i] = nullptr;
        s_matXqz[i]     = nullptr;
    }
    g_materialsReady = false;
    return false;
}

void Chams::Shutdown() {
    for (int i = 0; i < (int)ChamsStyle::COUNT; ++i) {
        s_matVisible[i] = nullptr;
        s_matXqz[i]     = nullptr;
    }
    g_materialsReady = false;
}

// ─── DrawArray hook ───────────────────────────────────────────────────────────
void __fastcall Chams::HookDrawArray(void* a1, void* a2, CMeshData* meshData, int nCount,
                                     void* sceneView, void* sceneLayer, void* u1, void* u2)
{
    if (!oDrawArray) return;

    // ── Diagnostic: prove the hook fires + dump material names ────────────────
    if (Globals::chams_debug && meshData) {
        ++s_dbgCalls;
        if ((s_dbgCalls % 300) == 1) {
            const char* nm = GetMaterialName(meshData->pMaterial);
            Log::Write("Chams DBG: call#%d mat=%p name=%s player=%d (hits=%d) mats=%d",
                       s_dbgCalls, meshData->pMaterial, nm ? nm : "(null)",
                       (int)IsPlayerMaterial(meshData->pMaterial), s_dbgPlayerHits,
                       (int)g_materialsReady);
        }
        if (IsPlayerMaterial(meshData->pMaterial)) ++s_dbgPlayerHits;
    }

    if (!meshData || !(Globals::enemy_chams_enabled || Globals::team_chams_enabled)) {
        oDrawArray(a1, a2, meshData, nCount, sceneView, sceneLayer, u1, u2);
        return;
    }

    if (!IsPlayerMaterial(meshData->pMaterial)) {
        oDrawArray(a1, a2, meshData, nCount, sceneView, sceneLayer, u1, u2);
        return;
    }

    const bool useEnemy = Globals::enemy_chams_enabled;
    const bool useTeam  = Globals::team_chams_enabled;
    if (!useEnemy && !useTeam) {
        oDrawArray(a1, a2, meshData, nCount, sceneView, sceneLayer, u1, u2);
        return;
    }

    // Pick config from enemy slot (takes priority) or team slot
    const float* visColor = useEnemy ? Globals::enemy_chams_visible_color   : Globals::team_chams_visible_color;
    const float* xqzColor = useEnemy ? Globals::enemy_chams_invisible_color : Globals::team_chams_invisible_color;
    bool useXqz           = useEnemy ? Globals::enemy_chams_xqz_enabled     : Globals::team_chams_xqz_enabled;
    int  styleIdx         = useEnemy ? Globals::enemy_chams_material         : Globals::team_chams_material;
    styleIdx = std::clamp(styleIdx, 0, (int)ChamsStyle::COUNT - 1);

    void* matVis = s_matVisible[styleIdx];
    void* matXqz = s_matXqz    [styleIdx];

    void* origMat   = meshData->pMaterial;
    DrawColor origCol = meshData->color;

    // XQZ pass first (draws through walls)
    if (useXqz && matXqz) {
        meshData->pMaterial = matXqz;
        ApplyColor(meshData, xqzColor);
        oDrawArray(a1, a2, meshData, nCount, sceneView, sceneLayer, u1, u2);
        meshData->pMaterial = origMat;
        meshData->color     = origCol;
    }

    // Visible pass (paints over XQZ in front of walls)
    if (matVis) meshData->pMaterial = matVis;
    ApplyColor(meshData, visColor);
    oDrawArray(a1, a2, meshData, nCount, sceneView, sceneLayer, u1, u2);

    // Restore
    meshData->pMaterial = origMat;
    meshData->color     = origCol;
}
