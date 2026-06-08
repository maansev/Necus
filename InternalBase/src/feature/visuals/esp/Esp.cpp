#include "Esp.h"
#include "../../../sdk/entity/EntityManager.h"
#include "../../../sdk/utils/Utils.h"
#include "../../../sdk/utils/Globals.h"
#include "../../../../ext/imgui/imgui.h"
#include <algorithm>
#include "WeaponNames.h"

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

    // --- СИНХРОННАЯ ЗАПИСЬ GLOW ШЕЙДЕРА VALVE ---
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

    // --- ОСНОВНОЙ ЦИКЛ ОБСЛУЖИВАНИЯ 2D ESP ВИЗУАЛОВ ---
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

        // СТРОГИЙ ФИКС ОШИБКИ ИКОНОК: Берем локальные моды оружия из меню без путаницы с общими Globals
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

        // 4. Weapon (Рендеринг Текста или Иконки на основе флага weaponMode из меню)
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

                        // Выбор режима отображения на основе флага из подвкладки меню
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

        // 5. Анатомический Скелет человека
        if (drawSkeleton)
        {
            const float thick = Globals::esp_skeleton_thickness;
            const float glowThick = thick + skelGlowTh;

            struct BoneConnection { int from; int to; };
            static const BoneConnection human_biped_bones[] = {
                { 6, 5 }, { 5, 4 }, { 4, 2 }, { 2, 0 },
                { 4, 8 }, { 8, 9 }, { 9, 11 },
                { 4, 13 }, { 13, 14 }, { 14, 16 },
                { 0, 22 }, { 22, 23 }, { 23, 24 },
                { 0, 25 }, { 25, 26 }, { 26, 27 }
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
}

void ESP::HandleModelGlow() {}