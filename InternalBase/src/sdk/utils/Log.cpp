#define _CRT_SECURE_NO_WARNINGS
#include "Log.h"
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <psapi.h>
#include <atomic>

#pragma comment(lib, "psapi.lib")

static HANDLE g_logFile = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_logCs;
static bool g_logCsInit = false;
static PVOID g_vehHandle = nullptr;
static LPTOP_LEVEL_EXCEPTION_FILTER g_prevUef = nullptr;
// thread_local: each thread (FSN, Present, etc.) has its own checkpoint
// so a crash in one thread doesn't report another thread's last checkpoint.
static thread_local const char* tl_checkpoint = "none";

// ─── Breadcrumb ring buffer ──────────────────────────────────────────────────
// Checkpoint() pushes here with ZERO file I/O (just two stores + one atomic
// increment) so it's free to call every frame — no FPS hit. On a crash the
// handler dumps the last N breadcrumbs, giving the full sequence of steps that
// led to the crash even though nothing was flushed to disk per-frame.
struct Crumb { DWORD tid; const char* name; };
static constexpr uint32_t kCrumbCount = 256; // power of two
static Crumb g_crumbs[kCrumbCount] = {};
static std::atomic<uint32_t> g_crumbIdx{ 0 };
static std::atomic<bool>     g_crumbsDumped{ false };

void Log::Checkpoint(const char* name) {
    tl_checkpoint = name;
    uint32_t i = g_crumbIdx.fetch_add(1, std::memory_order_relaxed);
    g_crumbs[i & (kCrumbCount - 1)] = { GetCurrentThreadId(), name };
}

// Dump the last breadcrumbs (oldest → newest). Called from crash handlers.
// Guarded so we only dump once even if multiple exceptions fire.
static void DumpCrumbs() {
    if (g_crumbsDumped.exchange(true)) return;
    uint32_t end = g_crumbIdx.load(std::memory_order_relaxed);
    uint32_t start = (end > kCrumbCount) ? (end - kCrumbCount) : 0;
    Log::Write("---- breadcrumb dump (last %u steps) ----", end - start);
    for (uint32_t k = start; k < end; ++k) {
        const Crumb& c = g_crumbs[k & (kCrumbCount - 1)];
        if (c.name)
            Log::Write("  [t%05lu] %s", c.tid, c.name);
    }
    Log::Write("---- end breadcrumb dump ----");
}

// Имя модуля + смещение для адреса (например "client.dll+0x12345")
static void DescribeAddress(void* addr, char* out, size_t outSz) {
    HMODULE mods[512]; DWORD needed = 0;
    HANDLE proc = GetCurrentProcess();
    if (EnumProcessModules(proc, mods, sizeof(mods), &needed)) {
        int count = needed / sizeof(HMODULE);
        for (int i = 0; i < count; ++i) {
            MODULEINFO mi{};
            if (!GetModuleInformation(proc, mods[i], &mi, sizeof(mi))) continue;
            uintptr_t base = (uintptr_t)mi.lpBaseOfDll;
            uintptr_t a = (uintptr_t)addr;
            if (a >= base && a < base + mi.SizeOfImage) {
                char name[MAX_PATH] = {};
                GetModuleBaseNameA(proc, mods[i], name, sizeof(name));
                snprintf(out, outSz, "%s+0x%llX", name, (unsigned long long)(a - base));
                return;
            }
        }
    }
    snprintf(out, outSz, "0x%p (unknown module)", addr);
}

// VEH: логируем фатальные краши + C++ исключения (0xE06D7363) которые могут
// вызвать terminate() без дополнительного логирования.
static LONG CALLBACK VehHandler(PEXCEPTION_POINTERS ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;

    // C++ exception thrown — log it so we can diagnose terminate() crashes.
    // 0xE06D7363 = Microsoft C++ EH magic. We only log, never swallow.
    if (code == 0xE06D7363) {
        char where[256] = {};
        DescribeAddress(ep->ExceptionRecord->ExceptionAddress, where, sizeof(where));
        Log::Write("!!! C++ EXCEPTION at %s | checkpoint=%s", where, (const char*)tl_checkpoint);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // пропускаем отладочные и SEH-probe исключения
    if (code != EXCEPTION_ACCESS_VIOLATION &&
        code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_STACK_OVERFLOW &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
        code != EXCEPTION_PRIV_INSTRUCTION)
        return EXCEPTION_CONTINUE_SEARCH;

    char where[256] = {};
    DescribeAddress(ep->ExceptionRecord->ExceptionAddress, where, sizeof(where));

    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
        const char* op = ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "read"
                       : ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "write" : "exec";
        Log::Write("!!! EXCEPTION code=0x%08X at %s — AV %s of 0x%p | checkpoint=%s",
                   code, where, op, (void*)ep->ExceptionRecord->ExceptionInformation[1],
                   (const char*)tl_checkpoint);
    } else {
        Log::Write("!!! EXCEPTION code=0x%08X at %s | checkpoint=%s",
                   code, where, (const char*)tl_checkpoint);
    }
    DumpCrumbs(); // full step-by-step trail leading to the crash
    return EXCEPTION_CONTINUE_SEARCH; // не глотаем — пусть SEH-обработчики работают
}

// Last-resort filter: fires when an exception goes fully unhandled and the
// process is about to die. VEH (above) sees first-chance exceptions, but if a
// crash kills the process before/around our SEH (e.g. unwind corruption through
// a hooked trampoline), this still gets a chance to dump the breadcrumb trail.
static LONG WINAPI UnhandledFilter(PEXCEPTION_POINTERS ep) {
    char where[256] = {};
    DescribeAddress(ep->ExceptionRecord->ExceptionAddress, where, sizeof(where));
    Log::Write("!!! UNHANDLED code=0x%08X at %s | checkpoint=%s",
               ep->ExceptionRecord->ExceptionCode, where, (const char*)tl_checkpoint);
    DumpCrumbs();
    return g_prevUef ? g_prevUef(ep) : EXCEPTION_CONTINUE_SEARCH;
}

void Log::Init() {
    if (!g_logCsInit) { InitializeCriticalSection(&g_logCs); g_logCsInit = true; }
    if (g_logFile != INVALID_HANDLE_VALUE) return;

    g_logFile = CreateFileA("C:\\necus_log.txt", GENERIC_WRITE, FILE_SHARE_READ,
                            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_logFile == INVALID_HANDLE_VALUE) {
        char path[MAX_PATH];
        GetTempPathA(MAX_PATH, path);
        lstrcatA(path, "necus_log.txt");
        g_logFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ,
                                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    g_vehHandle = AddVectoredExceptionHandler(1, VehHandler);
    g_prevUef = SetUnhandledExceptionFilter(UnhandledFilter);
    Write("=== Necus log started ===");
}

void Log::Write(const char* fmt, ...) {
    if (g_logFile == INVALID_HANDLE_VALUE || !g_logCsInit) return;

    char msg[1024];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(msg, sizeof(msg) - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;

    SYSTEMTIME st; GetLocalTime(&st);
    char line[1200];
    int len = snprintf(line, sizeof(line), "[%02d:%02d:%02d.%03d][t%05lu] %s\r\n",
                       st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                       GetCurrentThreadId(), msg);
    if (len <= 0) return;

    EnterCriticalSection(&g_logCs);
    DWORD written = 0;
    WriteFile(g_logFile, line, (DWORD)len, &written, nullptr);
    FlushFileBuffers(g_logFile);   // гарантируем запись до возможного краша
    LeaveCriticalSection(&g_logCs);
}

void Log::Shutdown() {
    if (g_vehHandle) { RemoveVectoredExceptionHandler(g_vehHandle); g_vehHandle = nullptr; }
    if (g_logFile != INVALID_HANDLE_VALUE) { CloseHandle(g_logFile); g_logFile = INVALID_HANDLE_VALUE; }
}
