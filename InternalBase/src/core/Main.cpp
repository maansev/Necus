#include <Windows.h>
#include <thread>
#include <chrono>
#include "Hooks.h"
#include "Config.h"
#include "../sdk/memory/PatternScan.h"
#include "../sdk/utils/Log.h"

static HANDLE g_MainThread = nullptr;

DWORD WINAPI MainThread(LPVOID module)
{
#ifdef _DEBUG
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
#endif

    // Create the log file + install the crash handler (VEH) IMMEDIATELY on inject,
    // before we wait on client.dll. Otherwise a crash during the wait / early setup
    // happens before Log::Init runs and nothing is ever written — which looks like
    // "the log never gets created". Init is idempotent (Hooks::Setup calls it again).
    Log::Init();
    Log::Write("MainThread start — waiting for client.dll");

    // Wait until client.dll is fully loaded before setting up hooks.
    // Without this, PatternScan returns 0 and all hooks silently fail on first inject.
    bool clientFound = false;
    for (int i = 0; i < 600; ++i) {
        if (Memory::GetModuleBase("client.dll")) { clientFound = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!clientFound) {
        Log::Write("client.dll NOT found after 60s — aborting setup");
        FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
        return 0;
    }
    Log::Write("client.dll found — settling 500ms then Hooks::Setup");
    // Extra settling time after the module appears (vtable init, etc.)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    Hooks::Setup();
    Config::Load("default");

    while (!(GetAsyncKeyState(VK_END) & 1))
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

    Config::Save("default");
    Hooks::Destroy();

#ifdef _DEBUG
    if (f) fclose(f);
    FreeConsole();
#endif

    FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        g_MainThread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    return TRUE;
}
