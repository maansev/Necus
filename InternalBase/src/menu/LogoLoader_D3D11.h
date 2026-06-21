// =============================================================================
//  LogoLoader_D3D11.h  Ч  creates the logo texture from RAW embedded RGBA pixels.
//
//  ?? Ётот загрузчик Ќ≈ использует WIC/COM/PNG-декодирование Ч поэтому он
//     не может крашить на render-потоке (стара€ верси€ падала именно там).
//     ѕиксели уже декодированы в logo_rgba.h (g_logo_rgba[], straight RGBA).
//
//  USAGE (вызвать один раз, когда есть ID3D11Device):
//      #include "LogoLoader_D3D11.h"
//      NecusSetLogo( LoadNecusLogo(pDevice) );
// =============================================================================
#pragma once

#include "../../ext/imgui/imgui.h"
#include "logo_rgba.h"       // g_logo_rgba[], g_logo_rgba_w/h/len
#include <d3d11.h>

void NecusSetLogo(ImTextureID tex);   // defined in Menu.cpp

// Returns an ImTextureID (ID3D11ShaderResourceView*) or 0 on failure.
static ImTextureID LoadNecusLogo(ID3D11Device* device)
{
    if (!device) return (ImTextureID)0;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = g_logo_rgba_w;
    td.Height = g_logo_rgba_h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem = g_logo_rgba;
    srd.SysMemPitch = g_logo_rgba_w * 4;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(device->CreateTexture2D(&td, &srd, &tex)) || !tex)
        return (ImTextureID)0;

    D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
    svd.Format = td.Format;
    svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    HRESULT hr = device->CreateShaderResourceView(tex, &svd, &srv);
    tex->Release();                       // SRV keeps its own ref to the texture
    if (FAILED(hr) || !srv)
        return (ImTextureID)0;

    return (ImTextureID)srv;              // ImGui DX11 backend uses the SRV as texture id
}
