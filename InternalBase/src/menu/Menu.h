#pragma once

struct ImFont;   // forward-decl — avoids pulling imgui.h into every includer

namespace Menu {
    inline bool IsOpen = false;
    void Render();
    void UpdateBinds();    // call every frame — applies bind logic to feature bools
    void RenderOverlays(); // call every frame (regardless of IsOpen) — watermark, keybinds, spectators
}

// ─────────────────────────────────────────────────────────────────────────────
//  Font loaders (defined in Menu.cpp).
//  Call ONCE at init: after ImGui::CreateContext() + backend init (ImGui_ImplDX11_Init),
//  and BEFORE the first ImGui::NewFrame().
//      NecusLoadFontEmbedded(18.0f);     // compiled-in Poppins (PoppinsFont.h) — no file needed
//      NecusLoadFont("C:/path/font.ttf", 18.0f);   // fallback: load from a file
//  If the menu already rendered with the default font, call
//  ImGui_ImplDX11_InvalidateDeviceObjects() once afterwards so the atlas rebuilds.
// ─────────────────────────────────────────────────────────────────────────────
ImFont* NecusLoadFontEmbedded(float sizePx = 18.0f);
ImFont* NecusLoadFont(const char* ttfPath, float sizePx = 18.0f);
