#define _CRT_SECURE_NO_WARNINGS
#include "Hooks.h"
#include "../../ext/imgui/imgui.h"
#include "../../ext/imgui/imgui_impl_win32.h"
#include "../../ext/imgui/imgui_impl_dx11.h"
#include "../../ext/minhook/MinHook.h"
#include "../../src/menu/Menu.h"
#include "../../src/menu/LogoLoader_D3D11.h"   // вшитый логотип (NecusSetLogo / LoadNecusLogo)
#include "../../src/feature/visuals/Visuals.h"
#include "../../src/feature/visuals/chams/Chams.h"
#include "../../src/sdk/entity/EntityManager.h"
#include "../../src/sdk/utils/Globals.h"
#include "../../src/sdk/memory/Offsets.h"
#include "../../src/sdk/memory/PatternScan.h"
#include "../feature/misc/Misc.h"
#include <wingdi.h>   // для SetDeviceGammaRamp

// Гамма
static bool  g_NightModeActive = false;
static WORD  g_OriginalGamma[3][256];

#pragma comment(lib, "d3d11.lib")

ID3D11Device* g_Device = nullptr;
static ID3D11DeviceContext* g_Context = nullptr;
static ID3D11RenderTargetView* g_RTV = nullptr;
static HWND g_Window = nullptr;
static bool g_Init = false;

extern void AddLog(const wchar_t* msg);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ==================== SDL HOOK ====================
using SDL_SetRelativeMouseModeFn = int(__cdecl*)(int enabled);
SDL_SetRelativeMouseModeFn oSDL_SetRelativeMouseMode = nullptr;

int __cdecl hkSDL_SetRelativeMouseMode(int enabled)
{
    if (Menu::IsOpen)
        return 0;
    return oSDL_SetRelativeMouseMode(enabled);
}

// ==================== CHAMS DRAW MODEL HOOK ====================
using DrawModelFn = void(__fastcall*)(void*, void*, void*, int, int, int, int);
DrawModelFn oDrawModel = nullptr;

void __fastcall hkDrawModel(void* thisptr, void* edx, void* renderInfo, int flags, int unknown1, int unknown2, int unknown3)
{
    // Здесь можно добавлять логику Chams
    // Пока просто вызываем оригинал
    oDrawModel(thisptr, edx, renderInfo, flags, unknown1, unknown2, unknown3);
}

// ==================== WNDPROC ====================
LRESULT __stdcall Hooks::hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (Menu::IsOpen)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) return true;
        if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_CHAR) return true;
        if (msg == WM_INPUT) return true;
    }
    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

// ==================== CreateMove ====================
bool __stdcall Hooks::hkCreateMove(float frameTime, void* cmd)
{
    const bool result = oCreateMove(frameTime, cmd);
    if (cmd && !Menu::IsOpen)
    {
        auto userCmd = reinterpret_cast<CUserCmd*>(cmd);
        Misc::RunBunnyHop(userCmd);
    }
    return result;
}

// ==================== Present ====================
HRESULT __stdcall Hooks::hkPresent(IDXGISwapChain* swapChain, UINT sync, UINT flags)
{
    if (!g_Init)
    {
        if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_Device)))
            return oPresent(swapChain, sync, flags);

        g_Device->GetImmediateContext(&g_Context);

        DXGI_SWAP_CHAIN_DESC sd{};
        swapChain->GetDesc(&sd);
        g_Window = sd.OutputWindow;

        ID3D11Texture2D* backBuffer = nullptr;
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        g_Device->CreateRenderTargetView(backBuffer, nullptr, &g_RTV);
        backBuffer->Release();

        oWndProc = (WNDPROC)SetWindowLongPtr(g_Window, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        ImGui_ImplWin32_Init(g_Window);
        ImGui_ImplDX11_Init(g_Device, g_Context);

        // === Embedded assets: load ONCE, before the first ImGui::NewFrame() ===
        NecusLoadFontEmbedded(18.0f);            // Poppins from PoppinsFont.h
        NecusSetLogo(LoadNecusLogo(g_Device));   // logo from logo.h

        // Инициализация чамсов происходит в Hooks::Setup()

        g_Init = true;
    }

    EntityManager::Get().Update();

    uintptr_t client = Memory::GetModuleBase("client.dll");
    if (client)
        memcpy(&Globals::ViewMatrix, (void*)(client + Offsets::dwViewMatrix), sizeof(Globals::ViewMatrix));

    if (GetAsyncKeyState(VK_INSERT) & 1)
    {
        Menu::IsOpen = !Menu::IsOpen;
        if (Menu::IsOpen)
        {
            ::ShowCursor(TRUE);
            ::ClipCursor(nullptr);
        }
        else
        {
            ::ShowCursor(FALSE);
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (Menu::IsOpen)
        Menu::Render();

    Visuals::Render();

    ImGui::Render();
    g_Context->OMSetRenderTargets(1, &g_RTV, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // --- Night Mode ---
    if (Globals::misc_nightmode)
    {
        if (!g_NightModeActive)
        {
            HDC hDC = GetDC(g_Window);
            GetDeviceGammaRamp(hDC, g_OriginalGamma);
            ReleaseDC(g_Window, hDC);
            g_NightModeActive = true;
        }

        HDC hDC = GetDC(g_Window);
        WORD ramp[3][256];
        float brightness = Globals::misc_nightmode_brightness;
        for (int i = 0; i < 256; i++)
        {
            WORD val = (WORD)(i * 256 * brightness);
            if (val > 65535) val = 65535;
            ramp[0][i] = ramp[1][i] = ramp[2][i] = val;
        }
        SetDeviceGammaRamp(hDC, ramp);
        ReleaseDC(g_Window, hDC);
    }
    else if (g_NightModeActive)
    {
        // Восстанавливаем оригинал
        HDC hDC = GetDC(g_Window);
        SetDeviceGammaRamp(hDC, g_OriginalGamma);
        ReleaseDC(g_Window, hDC);
        g_NightModeActive = false;
    }

    return oPresent(swapChain, sync, flags);
}

void Hooks::Setup()
{
    // Инициализируем материалы чамсов (интерфейс ресурсов)
    chams::Init();

    if (MH_Initialize() != MH_OK) return;

    // Установка хука DrawArray (чамсы)
    {
        uintptr_t drawArrayAddr = Memory::PatternScan("client.dll", "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC 30 49 8B F8");
        if (drawArrayAddr) {
            MH_CreateHook(reinterpret_cast<void*>(drawArrayAddr), &chams::DrawArrayHook, (void**)&oDrawArray);
        }
        // else { AddLog(L"Chams: DrawArray signature not found"); }  // <-- закомментируй или удали эту строку
    }

    using CreateInterfaceFn = void* (*)(const char*, int*);
    auto CreateInterface = (CreateInterfaceFn)GetProcAddress(GetModuleHandleA("engine2.dll"), "CreateInterface");

    if (CreateInterface)
    {
        void* client = CreateInterface("VClient017", nullptr);
        if (client)
        {
            void* clientMode = *(void**)((uintptr_t)client + 0x28);
            if (clientMode)
            {
                void** vmt = *(void***)clientMode;
                void* createMoveAddr = vmt[24];
                MH_CreateHook(createMoveAddr, &hkCreateMove, (void**)&oCreateMove);
            }
        }
    }

    // === SDL hook ===
    HMODULE sdlModule = GetModuleHandleA("SDL2.dll");
    if (sdlModule)
    {
        void* sdlSetRelative = GetProcAddress(sdlModule, "SDL_SetRelativeMouseMode");
        if (sdlSetRelative)
            MH_CreateHook(sdlSetRelative, &hkSDL_SetRelativeMouseMode, (void**)&oSDL_SetRelativeMouseMode);
    }

    // === CHAMS HOOK (DrawModel) ===
    HMODULE clientModule = GetModuleHandleA("client.dll");
    if (clientModule)
    {
        using CreateInterfaceFn = void* (*)(const char*, int*);
        auto CreateInterfaceClient = (CreateInterfaceFn)GetProcAddress(clientModule, "CreateInterface");

        if (CreateInterfaceClient)
        {
            void* modelRender = CreateInterfaceClient("VModelRender", nullptr);
            if (modelRender)
            {
                void** vmt = *(void***)modelRender;
                MH_CreateHook(vmt[21], &hkDrawModel, (void**)&oDrawModel);
            }
        }
    }

    // === Present hook ===
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"DummyDX";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* sc = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL fl;

    if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx)))
    {
        void** vtable = *reinterpret_cast<void***>(sc);
        void* presentAddr = vtable[8];
        MH_CreateHook(presentAddr, &hkPresent, reinterpret_cast<void**>(&oPresent));
        MH_EnableHook(MH_ALL_HOOKS);
        sc->Release();
        dev->Release();
        ctx->Release();
    }

    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

void Hooks::Destroy()
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (g_Window && oWndProc)
        SetWindowLongPtr(g_Window, GWLP_WNDPROC, (LONG_PTR)oWndProc);

    if (g_NightModeActive)
    {
        HDC hDC = GetDC(g_Window);
        SetDeviceGammaRamp(hDC, g_OriginalGamma);
        ReleaseDC(g_Window, hDC);
        g_NightModeActive = false;
    }

    if (!g_Init) return;

    // Выгружаем чамсы
    chams::Shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_RTV) g_RTV->Release();
    if (g_Context) g_Context->Release();
    if (g_Device) g_Device->Release();

    g_Init = false;
}