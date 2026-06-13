#pragma once
#include <Windows.h>

struct CUserCmd;
namespace Misc {
    void Render();
    void RunBunnyHop(CUserCmd* cmd);
    void RunAutoAccept(HWND gameWindow);
}