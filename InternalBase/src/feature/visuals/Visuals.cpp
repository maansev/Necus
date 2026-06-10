#define NOMINMAX
#include "Visuals.h"
#include "esp/Esp.h"
#include "enemycounter/EnemyCounter.h"
#include "chams/Chams.h"
#include "../../../ext/imgui/imgui.h"
#include "../../sdk/utils/Globals.h"
#include <cmath>

void Visuals::Render()
{
    ESP::Render();
    EnemyCounter::Render();

    if (Globals::aimbot_enabled && Globals::aimbot_draw_fov) {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const float sw = ImGui::GetIO().DisplaySize.x;
        const float sh = ImGui::GetIO().DisplaySize.y;
        float radius = tanf(Globals::aimbot_fov * 3.14159265f / 180.f) * (sw * 0.5f);
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
            Globals::aimbot_fov_color[0], Globals::aimbot_fov_color[1],
            Globals::aimbot_fov_color[2], Globals::aimbot_fov_color[3]));
        dl->AddCircle(ImVec2(sw * 0.5f, sh * 0.5f), radius, col, 64, 1.5f);
    }
}