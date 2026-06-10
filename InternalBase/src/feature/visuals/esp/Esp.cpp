#include "Esp.h"
#include "../../../sdk/entity/EntityManager.h"
#include "../../../sdk/utils/Utils.h"
#include "../../../sdk/utils/Globals.h"
#include "../../../../ext/imgui/imgui.h"
#include <algorithm>
#include "WeaponNames.h"

static void RenderWorldESP(ImDrawList* dl, float sw, float sh);
static void RenderGrenadeTrajectory(ImDrawList* dl, float sw, float sh, C_CSPlayerPawn* localPawn);

void ESP::Render()
{
    if (!Globals::esp_enabled)
        return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float sw = ImGui::GetIO().DisplaySize.x;
    const float sh = ImGui::GetIO().DisplaySize.y;

    const auto& entities = EntityManager::Get().GetEntities();
    C_CSPlayerPawn* localPawn = EntityManager::Get().GetLocalPawn();
    if (!localPawn)
        return;

    // --- ���������� ������ GLOW ������� VALVE ---
    for (const auto& ent : entities)
    {
        C_CSPlayerPawn* pawn = ent.pawn;
        if (!pawn || !pawn->IsAlive()) continue;

        bool isTeammate = (pawn->m_iTeamNum() == localPawn->m_iTeamNum());

        bool glowOn = isTeammate ? Globals::team_glow_enabled : Globals::enemy_glow_enabled;
        int glowStyle = isTeammate ? Globals::team_glow_style : Globals::enemy_glow_style;
        float glowBright = isTeammate ? Globals::team_glow_brightness : Globals::enemy_glow_brightness;
        float* glowCol = isTeammate ? Globals::team_glow_color : Globals::enemy_glow_color;

        uintptr_t glowPropAddr = reinterpret_cast<uintptr_t>(pawn) + Offsets::m_Glow;
        if (glowPropAddr)
        {
            if (glowOn)
            {
                if (isTeammate && !Globals::esp_teammates)
                {
                    *reinterpret_cast<bool*>(glowPropAddr + Offsets::m_bGlowing) = false;
                }
                else
                {
                    *reinterpret_cast<bool*>(glowPropAddr + Offsets::m_bGlowing) = true;
                    *reinterpret_cast<int32_t*>(glowPropAddr + Offsets::m_iGlowType) = glowStyle;

                    *reinterpret_cast<uint8_t*>(glowPropAddr + Offsets::m_glowColorOverride + 0) = static_cast<uint8_t>(std::clamp(glowCol[0] * glowBright * 255.f, 0.f, 255.f));
                    *reinterpret_cast<uint8_t*>(glowPropAddr + Offsets::m_glowColorOverride + 1) = static_cast<uint8_t>(std::clamp(glowCol[1] * glowBright * 255.f, 0.f, 255.f));
                    *reinterpret_cast<uint8_t*>(glowPropAddr + Offsets::m_glowColorOverride + 2) = static_cast<uint8_t>(std::clamp(glowCol[2] * glowBright * 255.f, 0.f, 255.f));
                    *reinterpret_cast<uint8_t*>(glowPropAddr + Offsets::m_glowColorOverride + 3) = static_cast<uint8_t>(std::clamp(glowCol[3] * 255.f, 0.f, 255.f));
                }
            }
            else
            {
                *reinterpret_cast<bool*>(glowPropAddr + Offsets::m_bGlowing) = false;
            }
        }
    }

    const ImU32 boxCol = ImGui::ColorConvertFloat4ToU32(ImVec4(Globals::esp_box_color[0], Globals::esp_box_color[1], Globals::esp_box_color[2], Globals::esp_box_color[3]));
    const ImU32 skelCol = ImGui::ColorConvertFloat4ToU32(ImVec4(Globals::esp_skeleton_color[0], Globals::esp_skeleton_color[1], Globals::esp_skeleton_color[2], Globals::esp_skeleton_color[3]));
    const ImU32 nameCol = ImGui::ColorConvertFloat4ToU32(ImVec4(Globals::esp_name_color[0], Globals::esp_name_color[1], Globals::esp_name_color[2], Globals::esp_name_color[3]));
    const ImU32 teammateBoxCol = ImGui::ColorConvertFloat4ToU32(ImVec4(Globals::esp_teammate_color[0], Globals::esp_teammate_color[1], Globals::esp_teammate_color[2], Globals::esp_teammate_color[3]));

    // --- �������� ���� ������������ 2D ESP �������� ---
    for (const auto& ent : entities)
    {
        C_CSPlayerPawn* pawn = ent.pawn;
        if (!pawn || !pawn->IsAlive()) continue;

        bool isTeammate = (pawn->m_iTeamNum() == localPawn->m_iTeamNum());
        if (isTeammate && !Globals::esp_teammates) continue;

        bool drawBox = isTeammate ? Globals::team_box : Globals::enemy_box;
        ImU32 boxClr = isTeammate ? teammateBoxCol : boxCol;
        bool drawSkeleton = isTeammate ? Globals::team_skeleton : Globals::enemy_skeleton;

        bool drawName = isTeammate ? Globals::team_name : Globals::enemy_name;
        ImU32 nameClr = isTeammate ? ImGui::ColorConvertFloat4ToU32(ImVec4(Globals::team_name_color[0], Globals::team_name_color[1], Globals::team_name_color[2], Globals::team_name_color[3])) : nameCol;
        int namePos = isTeammate ? Globals::team_name_position : Globals::enemy_name_position;

        bool drawHealth = isTeammate ? Globals::team_health : Globals::enemy_health;
        int healthPos = isTeammate ? Globals::team_health_position : Globals::enemy_health_position;

        bool drawWeapon = isTeammate ? Globals::team_weapon : Globals::enemy_weapon;
        int weaponPos = isTeammate ? Globals::team_weapon_position : Globals::enemy_weapon_position;

        // ������� ���� ������ ������: ����� ��������� ���� ������ �� ���� ��� �������� � ������ Globals
        int weaponMode = isTeammate ? Globals::team_weapon_display_mode : Globals::enemy_weapon_display_mode;

        ImU32 activeSkelClr = isTeammate ? ImGui::ColorConvertFloat4ToU32(ImVec4(Globals::team_skeleton_color[0], Globals::team_skeleton_color[1], Globals::team_skeleton_color[2], Globals::team_skeleton_color[3])) : skelCol;
        bool skelGlowOn = isTeammate ? Globals::team_glow_skeleton : Globals::enemy_glow_skeleton;
        float skelGlowTh = isTeammate ? Globals::team_glow_skel_thickness : Globals::enemy_glow_skel_thickness;
        ImU32 skelGlowClr = isTeammate ? ImGui::ColorConvertFloat4ToU32(ImVec4(Globals::team_glow_skel_color[0], Globals::team_glow_skel_color[1], Globals::team_glow_skel_color[2], Globals::team_glow_skel_color[3])) : ImGui::ColorConvertFloat4ToU32(ImVec4(Globals::enemy_glow_skel_color[0], Globals::enemy_glow_skel_color[1], Globals::enemy_glow_skel_color[2], Globals::enemy_glow_skel_color[3]));

        Vector feet = pawn->m_vOldOrigin();
        Vector head = Utils::GetBonePos(pawn, BoneID::Head);
        if (head.IsZero()) continue;

        head.z += 8.2f;
        Vector sFeet, sHead;
        if (!Utils::WorldToScreen(feet, sFeet, (float*)Globals::ViewMatrix, sw, sh) ||
            !Utils::WorldToScreen(head, sHead, (float*)Globals::ViewMatrix, sw, sh))
            continue;

        float h = sFeet.y - sHead.y;
        if (h < 5.f) continue;
        float w = h * 0.5f;
        float x = sHead.x - w * 0.5f;
        float y = sHead.y;

        float padTop = 2.f;
        float padBottom = 2.f;

        // 1. Box
        if (drawBox)
        {
            dl->AddRect({ x, y }, { x + w, y + h }, boxClr, 0.f, 0, Globals::esp_box_thickness);
            dl->AddRect({ x - 1, y - 1 }, { x + w + 1, y + h + 1 }, IM_COL32(0, 0, 0, 220));
        }

        // 2. Health
        if (drawHealth)
        {
            int hp = pawn->m_iHealth();
            float hpFrac = std::clamp(hp / 100.f, 0.f, 1.f);
            ImU32 hpColor = IM_COL32(0, 255, 0, 255);

            if (healthPos == 2) {
                dl->AddRectFilled({ x - 6, y - 1 }, { x - 2, y + h + 1 }, IM_COL32(0, 0, 0, 150));
                dl->AddRectFilled({ x - 5, y + h - (h * hpFrac) }, { x - 3, y + h }, hpColor);
            }
            else if (healthPos == 3) {
                dl->AddRectFilled({ x + w + 2, y - 1 }, { x + w + 6, y + h + 1 }, IM_COL32(0, 0, 0, 150));
                dl->AddRectFilled({ x + w + 3, y + h - (h * hpFrac) }, { x + w + 5, y + h }, hpColor);
            }
            else if (healthPos == 0) {
                dl->AddRectFilled({ x - 1, y - 6 }, { x + w + 1, y - 2 }, IM_COL32(0, 0, 0, 150));
                dl->AddRectFilled({ x, y - 5 }, { x + (w * hpFrac), y - 3 }, hpColor);
                padTop += 6.f;
            }
            else if (healthPos == 1) {
                dl->AddRectFilled({ x - 1, y + h + 2 }, { x + w + 1, y + h + 6 }, IM_COL32(0, 0, 0, 150));
                dl->AddRectFilled({ x, y + h + 3 }, { x + (w * hpFrac), y + h + 5 }, hpColor);
                padBottom += 6.f;
            }
        }

        // 3. Player Name
        if (drawName && ent.controller)
        {
            char nameBuf[64]{};
            uintptr_t namePtr = reinterpret_cast<uintptr_t>(ent.controller->m_iszPlayerName());
            if (Utils::SafeReadString(namePtr, nameBuf))
            {
                ImVec2 ts = ImGui::CalcTextSize(nameBuf);
                float textY = (namePos == 0) ? (y - ts.y - padTop) : (y + h + padBottom);
                dl->AddText({ x + (w - ts.x) * 0.5f, textY }, nameClr, nameBuf);

                if (namePos == 0) padTop += ts.y + 2.f;
                else padBottom += ts.y + 2.f;
            }
        }

        // 4. Weapon (��������� ������ ��� ������ �� ������ ����� weaponMode �� ����)
        if (drawWeapon)
        {
            uintptr_t weaponServices = *(uintptr_t*)((uintptr_t)pawn + Offsets::m_pWeaponServices);
            if (weaponServices)
            {
                uint32_t activeHandle = *(uint32_t*)(weaponServices + Offsets::m_hActiveWeapon);
                if (activeHandle && activeHandle != 0xFFFFFFFF)
                {
                    uintptr_t weaponEntity = EntityManager::Get().GetEntityFromHandle(activeHandle);
                    if (weaponEntity)
                    {
                        int itemIndex = *(uint16_t*)(weaponEntity + Offsets::m_AttributeManager + 0x50 + Offsets::m_iItemDefinitionIndex);
                        const char* weaponStr = "??";

                        // ����� ������ ����������� �� ������ ����� �� ���������� ����
                        if (weaponMode == 0) {
                            auto it = weaponNames.find(itemIndex);
                            if (it != weaponNames.end()) weaponStr = it->second;
                        }
                        else {
                            auto it = weaponIcons.find(itemIndex);
                            if (it != weaponIcons.end()) weaponStr = it->second;
                        }

                        ImVec2 ts = ImGui::CalcTextSize(weaponStr);
                        float textY = (weaponPos == 0) ? (y - ts.y - padTop) : (y + h + padBottom);
                        dl->AddText({ x + (w - ts.x) * 0.5f, textY }, nameClr, weaponStr);

                        if (weaponPos == 0) padTop += ts.y + 2.f;
                        else padBottom += ts.y + 2.f;
                    }
                }
            }
        }

        // 5. ������������� ������ ��������
        if (drawSkeleton)
        {
            const float thick = Globals::esp_skeleton_thickness;
            const float glowThick = thick + skelGlowTh;

            struct BoneConnection { int from; int to; };
            static const BoneConnection human_biped_bones[] = {
                // Spine
                { 7, 6 }, { 6, 23 }, { 23, 4 }, { 4, 3 }, { 3, 1 },
                // Left arm
                { 23, 8 }, { 8, 9 }, { 9, 10 }, { 10, 11 },
                // Right arm
                { 23, 12 }, { 12, 13 }, { 13, 14 }, { 14, 15 },
                // Left leg
                { 1, 17 }, { 17, 18 }, { 18, 19 },
                // Right leg
                { 1, 20 }, { 20, 21 }, { 21, 22 },
            };

            if (skelGlowOn)
            {
                for (const auto& conn : human_biped_bones)
                {
                    Vector b1 = Utils::GetBonePos(pawn, static_cast<BoneID>(conn.from));
                    Vector b2 = Utils::GetBonePos(pawn, static_cast<BoneID>(conn.to));
                    if (b1.IsZero() || b2.IsZero()) continue;

                    Vector sb1, sb2;
                    if (Utils::WorldToScreen(b1, sb1, (float*)Globals::ViewMatrix, sw, sh) &&
                        Utils::WorldToScreen(b2, sb2, (float*)Globals::ViewMatrix, sw, sh))
                    {
                        dl->AddCircleFilled({ sb1.x, sb1.y }, glowThick * 0.5f, skelGlowClr);
                        dl->AddCircleFilled({ sb2.x, sb2.y }, glowThick * 0.5f, skelGlowClr);
                        dl->AddLine({ sb1.x, sb1.y }, { sb2.x, sb2.y }, skelGlowClr, glowThick);
                    }
                }
            }

            for (const auto& conn : human_biped_bones)
            {
                Vector b1 = Utils::GetBonePos(pawn, static_cast<BoneID>(conn.from));
                Vector b2 = Utils::GetBonePos(pawn, static_cast<BoneID>(conn.to));
                if (b1.IsZero() || b2.IsZero()) continue;

                Vector sb1, sb2;
                if (Utils::WorldToScreen(b1, sb1, (float*)Globals::ViewMatrix, sw, sh) &&
                    Utils::WorldToScreen(b2, sb2, (float*)Globals::ViewMatrix, sw, sh))
                {
                    dl->AddCircleFilled({ sb1.x, sb1.y }, thick * 0.5f, activeSkelClr);
                    dl->AddCircleFilled({ sb2.x, sb2.y }, thick * 0.5f, activeSkelClr);
                    dl->AddLine({ sb1.x, sb1.y }, { sb2.x, sb2.y }, activeSkelClr, thick);
                }
            }
        }
    }

    RenderGrenadeTrajectory(dl, sw, sh, localPawn);
    RenderWorldESP(dl, sw, sh);
}

// POD-only struct — safe to return from __try scope
struct WorldEntReading {
    bool     valid = false;
    int      itemDef = 0;
    uint32_t ownerHandle = 0;
    float    ox = 0, oy = 0, oz = 0;
};
struct C4TimerReading {
    bool  ticking = false;
    float blowTime = 0.f;
    float timerLen = 0.f;
};

// SEH-safe: only POD reads — no C++ objects with destructors in scope
static WorldEntReading ReadWorldEnt(uintptr_t listPtr, int i) {
    WorldEntReading r;
    __try {
        uintptr_t listEntry = *reinterpret_cast<uintptr_t*>(listPtr + (8 * ((i & 0x7FFF) >> 9)) + 16);
        if (!listEntry) return r;
        uintptr_t ent = *reinterpret_cast<uintptr_t*>(listEntry + 112 * (i & 0x1FF));
        if (!ent) return r;
        r.itemDef = (int)(*(uint16_t*)(ent + Offsets::m_AttributeManager + 0x50 + Offsets::m_iItemDefinitionIndex));
        r.ownerHandle = *(uint32_t*)(ent + Offsets::m_hOwnerEntity);
        float* orig = reinterpret_cast<float*>(ent + Offsets::m_vOldOrigin);
        r.ox = orig[0]; r.oy = orig[1]; r.oz = orig[2];
        r.valid = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return r;
}

static C4TimerReading ReadC4Timer(uintptr_t listPtr, int i) {
    C4TimerReading r;
    __try {
        uintptr_t listEntry = *reinterpret_cast<uintptr_t*>(listPtr + (8 * ((i & 0x7FFF) >> 9)) + 16);
        if (!listEntry) return r;
        uintptr_t ent = *reinterpret_cast<uintptr_t*>(listEntry + 112 * (i & 0x1FF));
        if (!ent) return r;
        r.ticking = *(bool*)(ent + Offsets::m_bBombTicking);
        r.blowTime = *(float*)(ent + Offsets::m_flC4Blow);
        r.timerLen = *(float*)(ent + Offsets::m_flTimerLength);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return r;
}

static uintptr_t ReadListPtr(uintptr_t addr) {
    uintptr_t v = 0;
    __try { v = *reinterpret_cast<uintptr_t*>(addr); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return v;
}

static void RenderGrenadeTrajectory(ImDrawList* dl, float sw, float sh, C_CSPlayerPawn* localPawn) {
    if (!Globals::grenade_trajectory) return;
    if (!localPawn || !localPawn->IsAlive()) return;

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;

    // Check active weapon is a grenade (item IDs 43-48)
    uintptr_t weapSvc = 0;
    int itemDef = 0;
    __try {
        weapSvc = *(uintptr_t*)((uintptr_t)localPawn + Offsets::m_pWeaponServices);
        if (!weapSvc) return;
        uint32_t activeHandle = *(uint32_t*)(weapSvc + Offsets::m_hActiveWeapon);
        if (!activeHandle || activeHandle == 0xFFFFFFFFu) return;
        uintptr_t listPtr = *(uintptr_t*)(client + Offsets::dwEntityList);
        if (!listPtr) return;
        uintptr_t entry = *(uintptr_t*)(listPtr + 8 * ((activeHandle & 0x7FFF) >> 9) + 16);
        if (!entry) return;
        uintptr_t weapEnt = *(uintptr_t*)(entry + 112 * (activeHandle & 0x1FF));
        if (!weapEnt) return;
        itemDef = (int)(*(uint16_t*)(weapEnt + Offsets::m_AttributeManager + 0x50 + Offsets::m_iItemDefinitionIndex));
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return; }

    if (itemDef < 43 || itemDef > 48) return;

    // Read view angles
    float pitch = 0.f, yaw = 0.f;
    __try {
        float* va = reinterpret_cast<float*>(client + Offsets::dwViewAngles);
        pitch = va[0] * 3.14159265f / 180.f;
        yaw = va[1] * 3.14159265f / 180.f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return; }

    // Eye position
    Vector origin = localPawn->m_vOldOrigin();
    Vector viewOff = localPawn->m_vecViewOffset();
    Vector eye = { origin.x + viewOff.x, origin.y + viewOff.y, origin.z + viewOff.z };

    // Throw velocity ~ 750 u/s in view direction + 200 u/s upward bias
    float cosp = cosf(-pitch), sinp = sinf(-pitch);
    float cosy = cosf(yaw), siny = sinf(yaw);
    const float kSpeed = 750.f;
    Vector vel = { cosp * cosy * kSpeed, cosp * siny * kSpeed, (-sinp * kSpeed) + 200.f };

    const float gravity = 800.f;
    const float dt = 0.05f;
    const int   steps = 100;  // 5 seconds

    ImVec2 prevSc{};
    bool hasPrev = false;

    for (int s = 0; s < steps; s++) {
        float t = s * dt;
        Vector pos = {
            eye.x + vel.x * t,
            eye.y + vel.y * t,
            eye.z + vel.z * t - 0.5f * gravity * t * t
        };

        Vector sc;
        if (!Utils::WorldToScreen(pos, sc, (float*)Globals::ViewMatrix, sw, sh)) {
            hasPrev = false;
            continue;
        }

        if (hasPrev) {
            float alpha = 1.f - (float)s / steps;
            ImU32 col = IM_COL32(255, 200, 50, (int)(alpha * 220.f));
            dl->AddLine(prevSc, ImVec2(sc.x, sc.y), col, 1.8f);
        }
        prevSc = ImVec2(sc.x, sc.y);
        hasPrev = true;
    }
}

static bool IsValidItemID(int id) {
    return (id >= 1 && id <= 68) || (id >= 500 && id <= 595);
}

// Returns true if ownerHandle points to a living player — means the weapon is currently held
static bool IsOwnerAlive(uintptr_t listPtr, uint32_t ownerHandle)
{
    if (!ownerHandle || ownerHandle == 0xFFFFFFFFu) return false;
    __try {
        uintptr_t entry = *reinterpret_cast<uintptr_t*>(listPtr + (8 * ((ownerHandle & 0x7FFF) >> 9)) + 16);
        if (!entry) return false;
        uintptr_t ent = *reinterpret_cast<uintptr_t*>(entry + 112 * (ownerHandle & 0x1FF));
        if (!ent) return false;
        int hp = *reinterpret_cast<int*>(ent + Offsets::m_iHealth);
        return hp > 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void RenderWorldESP(ImDrawList* dl, float sw, float sh)
{
    if (!Globals::world_dropped_weapons && !Globals::world_grenades && !Globals::world_c4)
        return;

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (!client) return;

    uintptr_t listPtr = ReadListPtr(client + Offsets::dwEntityList);
    if (!listPtr) return;

    ImU32 weapCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
        Globals::world_weapons_color[0], Globals::world_weapons_color[1],
        Globals::world_weapons_color[2], Globals::world_weapons_color[3]));
    ImU32 grenCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
        Globals::world_grenades_color[0], Globals::world_grenades_color[1],
        Globals::world_grenades_color[2], Globals::world_grenades_color[3]));
    ImU32 c4Col = IM_COL32(255, 60, 60, 255);

    for (int i = 1; i < 2048; ++i)
    {
        WorldEntReading r = ReadWorldEnt(listPtr, i);
        if (!r.valid) continue;
        if (!IsValidItemID(r.itemDef)) continue;
        if (fabsf(r.ox) > 16384.f || fabsf(r.oy) > 16384.f || fabsf(r.oz) > 16384.f) continue;
        // Skip weapons currently held by a live player
        if (IsOwnerAlive(listPtr, r.ownerHandle)) continue;

        bool isC4 = (r.itemDef == 49);
        bool isGrenade = (r.itemDef >= 43 && r.itemDef <= 48);
        bool isWeapon = (!isC4 && !isGrenade);

        if (isC4 && !Globals::world_c4)              continue;
        if (isGrenade && !Globals::world_grenades)         continue;
        if (isWeapon && !Globals::world_dropped_weapons)  continue;

        Vector origin(r.ox, r.oy, r.oz);
        if (origin.IsZero()) continue;

        Vector screen;
        if (!Utils::WorldToScreen(origin, screen, (float*)Globals::ViewMatrix, sw, sh)) continue;

        const char* name = "?";
        if (isC4) {
            name = "C4";
        }
        else {
            auto it = weaponNames.find(r.itemDef);
            if (it != weaponNames.end()) name = it->second;
        }

        ImU32 col = isC4 ? c4Col : (isGrenade ? grenCol : weapCol);

        ImVec2 ts = ImGui::CalcTextSize(name);
        float px = screen.x - ts.x * 0.5f - 4.f;
        float py = screen.y - ts.y * 0.5f - 2.f;
        dl->AddRectFilled(ImVec2(px, py), ImVec2(px + ts.x + 8.f, py + ts.y + 4.f),
            IM_COL32(0, 0, 0, 140), 3.f);
        dl->AddText(ImVec2(px + 4.f, py + 2.f), col, name);

        // HE grenade radius ring
        if (isGrenade && Globals::world_grenade_radius && r.itemDef == 44) {
            Vector right(r.ox + 350.f, r.oy, r.oz);
            Vector screenRight;
            if (Utils::WorldToScreen(right, screenRight, (float*)Globals::ViewMatrix, sw, sh)) {
                float screenRadius = fabsf(screenRight.x - screen.x);
                if (screenRadius > 4.f && screenRadius < 800.f)
                    dl->AddCircle(ImVec2(screen.x, screen.y), screenRadius, grenCol, 48, 1.2f);
            }
        }

        // C4 timer
        if (isC4 && Globals::world_c4_timer) {
            C4TimerReading ct = ReadC4Timer(listPtr, i);
            if (ct.ticking && ct.timerLen > 0.f && ct.blowTime > 0.f) {
                dl->AddText(ImVec2(screen.x - ImGui::CalcTextSize("BOMB").x * 0.5f, screen.y + ts.y + 6.f),
                    IM_COL32(255, 80, 80, 255), "BOMB");
            }
        }
    }
}

void ESP::HandleModelGlow() {}