#pragma once
#include <d3d11.h>
#include <dxgi.h>

namespace Hooks {
    void Setup();
    void Destroy();

    // Present
    inline HRESULT(__stdcall* oPresent)(IDXGISwapChain*, UINT, UINT) = nullptr;
    HRESULT __stdcall hkPresent(IDXGISwapChain*, UINT, UINT);

    // WndProc
    inline WNDPROC oWndProc = nullptr;
    LRESULT __stdcall hkWndProc(HWND, UINT, WPARAM, LPARAM);

    // CreateMove
    using CreateMoveFn = bool(__stdcall*)(float, void*);
    inline CreateMoveFn oCreateMove = nullptr;
    bool __stdcall hkCreateMove(float frameTime, void* cmd);
}