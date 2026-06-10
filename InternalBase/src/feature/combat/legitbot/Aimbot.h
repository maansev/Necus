#pragma once

class Aimbot {
public:
    static void Run(int stage);
    static bool HasTarget();  // used by GetRawInputData hook for lock_mouse
};
