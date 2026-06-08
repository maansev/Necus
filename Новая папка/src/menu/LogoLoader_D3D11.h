// =============================================================================
//  LogoLoader_D3D11.h  —  loads the embedded logo.png into a D3D11 texture
//  and hands it to the menu via NecusSetLogo().
//
//  No external libraries: decodes the PNG with Windows WIC (ships with Windows).
//
//  USAGE (call once at init, after your ID3D11Device exists):
//      #include "LogoLoader_D3D11.h"
//      NecusSetLogo( LoadNecusLogo(pDevice) );
//
//  If you render with OpenGL or Vulkan instead of D3D11, ping me and I'll
//  hand you the matching loader — only this file changes, Menu.cpp stays.
// =============================================================================
#pragma once

#include "../../ext/imgui/imgui.h"
#include "logo.h"            // embedded PNG bytes: g_logo_png[], g_logo_png_len

#include <d3d11.h>
#include <wincodec.h>        // WIC
#include <wrl/client.h>      // Microsoft::WRL::ComPtr
#include <vector>
#pragma comment(lib, "windowscodecs.lib")

void NecusSetLogo(ImTextureID tex);   // defined in Menu.cpp

// Returns an ImTextureID (ID3D11ShaderResourceView*) or 0 on failure.
static ImTextureID LoadNecusLogo(ID3D11Device* device)
{
    using Microsoft::WRL::ComPtr;
    if (!device) return (ImTextureID)0;

    // COM may already be initialized by the host; tolerate either state.
    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool needUninit = SUCCEEDED(hrInit);

    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)))) {
        if (needUninit) CoUninitialize();
        return (ImTextureID)0;
    }

    ComPtr<IWICStream> stream;
    factory->CreateStream(&stream);
    stream->InitializeFromMemory(const_cast<BYTE*>(g_logo_png), g_logo_png_len);

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr,
        WICDecodeMetadataCacheOnLoad, &decoder))) {
        if (needUninit) CoUninitialize();
        return (ImTextureID)0;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    decoder->GetFrame(0, &frame);

    // Convert to straight 32-bit RGBA.
    ComPtr<IWICFormatConverter> conv;
    factory->CreateFormatConverter(&conv);
    conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT w = 0, h = 0;
    conv->GetSize(&w, &h);
    std::vector<BYTE> pixels((size_t)w * h * 4);
    conv->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data());

    // Create the texture.
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = pixels.data();
    sd.SysMemPitch = w * 4;

    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(device->CreateTexture2D(&td, &sd, &tex))) {
        if (needUninit) CoUninitialize();
        return (ImTextureID)0;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
    svd.Format = td.Format;
    svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    device->CreateShaderResourceView(tex.Get(), &svd, &srv);   // srv keeps its own ref

    if (needUninit) CoUninitialize();
    return (ImTextureID)srv;   // ImGui DX11 backend expects the SRV as the texture id
}
