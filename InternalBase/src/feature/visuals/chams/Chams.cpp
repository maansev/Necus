#include "Chams.h"
#include "../../../sdk/utils/Globals.h"
#include "../../../sdk/memory/Offsets.h"
#include "../../../sdk/memory/PatternScan.h"
#include "../../../sdk/entity/EntityManager.h"
#include "../../../sdk/entity/Classes.h"
#include "../../../sdk/utils/Log.h"
#include <Windows.h>
#include <algorithm>

// ─── CS2 KV3 / Material API ──────────────────────────────────────────────────
// CS2 creates render materials from KV3 text (VMAT format).
// Two functions are needed from CS2 dlls:
//   1. AllocKV3()          — allocates a blank CKeyValues3 node  (tier1.dll)
//   2. KV3LoadBuffer()     — parses VMAT text into that node       (tier1.dll)
//   3. CreateMaterial()    — creates CMaterial2 from the KV3 node (materialsystem2.dll)

using AllocKV3Fn = void* (__fastcall*)();
using KV3LoadBufferFn = bool(__fastcall*)(void* kv3, const char* buf, size_t len, int flags, const char* debugName);
using CreateMaterialFn = void(__fastcall*)(void* unk, void** outMat, const char* name, void* kv3, uint64_t f0, uint64_t f1);

static AllocKV3Fn       s_AllocKV3 = nullptr;
static KV3LoadBufferFn  s_KV3LoadBuffer = nullptr;
static CreateMaterialFn s_CreateMaterial = nullptr;

// Material slots: for each ChamsStyle we have a visible layer and an XQZ layer.
static void* s_matVisible[(int)ChamsStyle::COUNT] = {};
static void* s_matXqz[(int)ChamsStyle::COUNT] = {};

// ─── VMAT text definitions ────────────────────────────────────────────────────
// These are CS2-format .vmat files embedded as strings.
// g_vColorTint is set at runtime (overwritten on mesh draw).

// FLAT — csgo_unlitgeneric, solid fill, respects Z
static const char kVmatFlat[] =
R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    Shader = "csgo_unlitgeneric.vfx"
    F_IGNOREZ = 0
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    g_vColorTint = [1.000000, 1.000000, 1.000000, 1.000000]
    g_tColor = resource:"materials/dev/primary_white_color_tga_f7b257f6.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
})";

// FLAT XQZ — same but ignores Z (draws through walls)
static const char kVmatFlatXqz[] =
R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    Shader = "csgo_unlitgeneric.vfx"
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    g_vColorTint = [1.000000, 1.000000, 1.000000, 0.450000]
    g_tColor = resource:"materials/dev/primary_white_color_tga_f7b257f6.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
})";

// ILLUMINATE — csgo_complex + self-illum, brighter/emissive look
static const char kVmatIllum[] =
R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    g_vColorTint = [1.000000, 1.000000, 1.000000, 1.000000]
    g_flSelfIllumScale = [3.000000, 3.000000, 3.000000, 3.000000]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})";

static const char kVmatIllumXqz[] =
R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_DISABLE_Z_BUFFERING = 1
    g_vColorTint = [1.000000, 1.000000, 1.000000, 0.450000]
    g_flSelfIllumScale = [3.000000, 3.000000, 3.000000, 3.000000]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})";

// GLOW_XQZ — csgo_effects fresnel rim (standard CS2 chams look)
static const char kVmatGlow[] =
R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_flFresnelExponent = 7.0
    g_flFresnelFalloff = 10.0
    g_flFresnelMax = 0.1
    g_flFresnelMin = 1.0
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_vColorTint = [1.000000, 1.000000, 1.000000, 1.000000]
    F_IGNOREZ = 0
    F_TRANSLUCENT = 1
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
})";

static const char kVmatGlowXqz[] =
R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_flFresnelExponent = 7.0
    g_flFresnelFalloff = 10.0
    g_flFresnelMax = 0.1
    g_flFresnelMin = 1.0
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_vColorTint = [1.000000, 1.000000, 1.000000, 0.000000]
    F_IGNOREZ = 1
    F_TRANSLUCENT = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
})";

// ─── Material creation helper ─────────────────────────────────────────────────
static void* CreateMaterial(const char* name, const char* vmatText) {
    if (!s_AllocKV3 || !s_KV3LoadBuffer || !s_CreateMaterial) return nullptr;

    void* kv3 = nullptr;
    __try { kv3 = s_AllocKV3(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    if (!kv3) return nullptr;

    bool loaded = false;
    __try { loaded = s_KV3LoadBuffer(kv3, vmatText, strlen(vmatText), 0, name); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    if (!loaded) return nullptr;

    void* mat = nullptr;
    __try { s_CreateMaterial(nullptr, &mat, name, kv3, 0, 1); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }

    return mat;
}

// ─── Entity handle from scene animatable object ───────────────────────────────
// CS2 CAnimatableSceneObject stores the owning entity handle at +0x28.
// Read it with SEH protection; entity lookup uses our EntityManager.
static uint32_t GetSceneOwnerHandle(void* sceneObj) {
    if (!sceneObj) return 0xFFFFFFFF;
    __try { return *(uint32_t*)((uintptr_t)sceneObj + 0x28); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0xFFFFFFFF; }
}

// ─── Apply tint colour to a CMeshData ────────────────────────────────────────
static inline void ApplyColor(CMeshData* m, const float* col4) {
    m->color.r = (uint8_t)std::clamp(col4[0] * 255.f, 0.f, 255.f);
    m->color.g = (uint8_t)std::clamp(col4[1] * 255.f, 0.f, 255.f);
    m->color.b = (uint8_t)std::clamp(col4[2] * 255.f, 0.f, 255.f);
    m->color.a = (uint8_t)std::clamp(col4[3] * 255.f, 0.f, 255.f);
}

// ─── Chams::Init ─────────────────────────────────────────────────────────────
bool Chams::Init() {
    // ── Resolve API functions ─────────────────────────────────────────────────
    // AllocKV3 — allocates a fresh CKeyValues3 node in tier1.dll
    // Pattern covers the prologue of CKeyValues3::AllocMaterial / KV3AllocMaterial
    uintptr_t addr = Memory::PatternScan("tier1.dll",
        "48 89 5C 24 08 57 48 83 EC 20 33 FF 48 8D 1D");
    Log::Write("Chams::Init AllocKV3 scan=0x%llX", (unsigned long long)addr);
    if (addr) s_AllocKV3 = reinterpret_cast<AllocKV3Fn>(addr);

    // KV3 LoadFromBuffer — parses VMAT text into the KV3 node
    addr = Memory::PatternScan("tier1.dll",
        "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 49 8B F0 48 8B DA 48 8B F9");
    Log::Write("Chams::Init KV3LoadBuf scan=0x%llX", (unsigned long long)addr);
    if (addr) s_KV3LoadBuffer = reinterpret_cast<KV3LoadBufferFn>(addr);

    // CreateMaterial — in materialsystem2.dll; creates CMaterial2 from KV3
    addr = Memory::PatternScan("materialsystem2.dll",
        "55 48 83 EC 50 48 8B EC 48 89 9C 24 78 01 00 00");
    if (!addr)
        addr = Memory::PatternScan("materialsystem2.dll",
            "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 30 48 8B 1A");
    Log::Write("Chams::Init CreateMaterial scan=0x%llX", (unsigned long long)addr);
    if (addr) s_CreateMaterial = reinterpret_cast<CreateMaterialFn>(addr);

    if (!s_AllocKV3 || !s_KV3LoadBuffer || !s_CreateMaterial) {
        Log::Write("Chams::Init material API not found — color-only mode");
        // Still mark ready; the hook will fall through without material swap
        // but will still apply vertex colour tinting.
        g_materialsReady = false;
        return false;
    }

    // ── Create materials ──────────────────────────────────────────────────────
    s_matVisible[(int)ChamsStyle::FLAT] = CreateMaterial("necus/chams_flat_v.vmat", kVmatFlat);
    s_matXqz[(int)ChamsStyle::FLAT] = CreateMaterial("necus/chams_flat_x.vmat", kVmatFlatXqz);
    s_matVisible[(int)ChamsStyle::ILLUMINATE] = CreateMaterial("necus/chams_illum_v.vmat", kVmatIllum);
    s_matXqz[(int)ChamsStyle::ILLUMINATE] = CreateMaterial("necus/chams_illum_x.vmat", kVmatIllumXqz);
    s_matVisible[(int)ChamsStyle::GLOW_XQZ] = CreateMaterial("necus/chams_glow_v.vmat", kVmatGlow);
    s_matXqz[(int)ChamsStyle::GLOW_XQZ] = CreateMaterial("necus/chams_glow_x.vmat", kVmatGlowXqz);

    int created = 0;
    for (int i = 0; i < (int)ChamsStyle::COUNT; ++i) {
        if (s_matVisible[i]) ++created;
        if (s_matXqz[i])    ++created;
    }
    Log::Write("Chams::Init created %d/6 materials", created);
    g_materialsReady = (created > 0);
    return g_materialsReady;
}

void Chams::Shutdown() {
    for (int i = 0; i < (int)ChamsStyle::COUNT; ++i) {
        s_matVisible[i] = nullptr;
        s_matXqz[i] = nullptr;
    }
    g_materialsReady = false;
}

// ─── DrawArray hook ───────────────────────────────────────────────────────────
void __fastcall Chams::HookDrawArray(void* a1, void* a2, CMeshData* meshData, int nCount,
    void* sceneView, void* sceneLayer, void* u1, void* u2)
{
    // Always have a valid original pointer before doing anything
    if (!oDrawArray) return;

    // Nothing enabled → skip all chams logic
    const bool anyEnabled = Globals::enemy_chams_enabled || Globals::team_chams_enabled;
    if (!anyEnabled || !meshData) {
        oDrawArray(a1, a2, meshData, nCount, sceneView, sceneLayer, u1, u2);
        return;
    }

    // Resolve entity from scene object's owner handle (offset 0x28 in CAnimatableSceneObject)
    uint32_t ownerHandle = GetSceneOwnerHandle(meshData->pSceneAnimatableObject);
    if (ownerHandle == 0xFFFFFFFF || ownerHandle == 0) {
        oDrawArray(a1, a2, meshData, nCount, sceneView, sceneLayer, u1, u2);
        return;
    }

    uintptr_t entityPtr = EntityManager::Get().GetEntityFromHandle(ownerHandle);
    if (!entityPtr) {
        oDrawArray(a1, a2, meshData, nCount, sceneView, sceneLayer, u1, u2);
        return;
    }

    // Look up entity in our cached list
    bool isEnemy = false, isTeam = false;
    {
        const auto& ents = EntityManager::Get().GetEntities();
        for (const auto& e : ents) {
            if (reinterpret_cast<uintptr_t>(e.pawn) == entityPtr) {
                isEnemy = e.isEnemy;
                isTeam = !e.isEnemy;
                break;
            }
        }
    }

    bool applyChams = (isEnemy && Globals::enemy_chams_enabled)
        || (isTeam && Globals::team_chams_enabled);
    if (!applyChams) {
        oDrawArray(a1, a2, meshData, nCount, sceneView, sceneLayer, u1, u2);
        return;
    }

    const float* visColor = isEnemy ? Globals::enemy_chams_visible_color : Globals::team_chams_visible_color;
    const float* xqzColor = isEnemy ? Globals::enemy_chams_invisible_color : Globals::team_chams_invisible_color;
    bool useXqz = isEnemy ? Globals::enemy_chams_xqz_enabled : Globals::team_chams_xqz_enabled;
    int  styleIdx = isEnemy ? Globals::enemy_chams_material : Globals::team_chams_material;
    styleIdx = std::clamp(styleIdx, 0, (int)ChamsStyle::COUNT - 1);

    void* matVis = s_matVisible[styleIdx];
    void* matXqz = s_matXqz[styleIdx];

    // ── XQZ pass (through-wall, drawn first so visible layer paints over it) ──
    if (useXqz && matXqz) {
        void* origMat = meshData->pMaterial;
        DrawColor origCol = meshData->color;

        meshData->pMaterial = matXqz;
        ApplyColor(meshData, xqzColor);
        oDrawArray(a1, a2, meshData, nCount, sceneView, sceneLayer, u1, u2);

        meshData->pMaterial = origMat;
        meshData->color = origCol;
    }

    // ── Visible pass ──────────────────────────────────────────────────────────
    {
        void* origMat = meshData->pMaterial;
        DrawColor origCol = meshData->color;

        if (matVis) meshData->pMaterial = matVis;
        ApplyColor(meshData, visColor);
        oDrawArray(a1, a2, meshData, nCount, sceneView, sceneLayer, u1, u2);

        meshData->pMaterial = origMat;
        meshData->color = origCol;
    }
}
