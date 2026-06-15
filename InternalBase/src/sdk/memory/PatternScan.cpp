#include "PatternScan.h"
#include <Windows.h>
#include <Psapi.h>
#include <vector>

uintptr_t Memory::GetModuleBase(const char* module)
{
    // client.dll is loaded once and never moves — cache its base to avoid
    // repeated GetModuleHandleA calls in per-frame hot paths (hkPresent,
    // hkFrameStageNotify, HookDrawArray all call this for client.dll).
    static uintptr_t s_clientCache = 0;
    if (module[0] == 'c' && module[1] == 'l') { // "client.dll" fast path
        if (s_clientCache) return s_clientCache;
        s_clientCache = reinterpret_cast<uintptr_t>(GetModuleHandleA(module));
        return s_clientCache;
    }
    return reinterpret_cast<uintptr_t>(GetModuleHandleA(module));
}

static std::vector<int> PatternToBytes(const char* pattern)
{
    std::vector<int> bytes;

    for (const char* c = pattern; *c; ++c)
    {
        if (*c == '?')
        {
            ++c;
            if (*c == '?') ++c;
            bytes.push_back(-1);
        }
        else
        {
            bytes.push_back(strtoul(c, const_cast<char**>(&c), 16));
        }
    }

    return bytes;
}

uintptr_t Memory::PatternScan(const char* module, const char* signature)
{
    HMODULE mod = GetModuleHandleA(module);
    if (!mod) return 0;

    MODULEINFO info{};
    if (!GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info)))
        return 0;

    auto base = reinterpret_cast<uint8_t*>(mod);
    auto size = info.SizeOfImage;

    auto pattern = PatternToBytes(signature);
    auto data = pattern.data();
    auto len = pattern.size();

    for (size_t i = 0; i < size - len; ++i)
    {
        bool found = true;
        for (size_t j = 0; j < len; ++j)
        {
            if (data[j] != -1 && data[j] != base[i + j])
            {
                found = false;
                break;
            }
        }
        if (found)
            return reinterpret_cast<uintptr_t>(&base[i]);
    }

    return 0;
}