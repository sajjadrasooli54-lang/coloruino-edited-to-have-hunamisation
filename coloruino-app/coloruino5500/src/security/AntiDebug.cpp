#include "AntiDebug.h"
#include "util/DynamicApi.h"

#include <windows.h>
#include <winternl.h>
#include <intrin.h>
#include <thread>
#include <chrono>

#pragma comment(lib, "ntdll.lib")

std::atomic<uint64_t> g_AuthHash{ 0 };

// ─── Dynamic NtQueryInformationProcess ──────────────────────────────────
typedef NTSTATUS (WINAPI *NtQueryInfoProcess_t)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
static NtQueryInfoProcess_t pNtQIP = nullptr;

static bool InitNtQIP() {
    if (pNtQIP) return true;
    HMODULE ntdll = dynamic_api::pGetModuleHandleA("ntdll.dll");
    if (!ntdll) return false;
    pNtQIP = (NtQueryInfoProcess_t)dynamic_api::pGetProcAddress(ntdll, "NtQueryInformationProcess");
    return pNtQIP != nullptr;
}

static constexpr PROCESSINFOCLASS kDebugPort = (PROCESSINFOCLASS)7;
static constexpr PROCESSINFOCLASS kDebugObjHandle = (PROCESSINFOCLASS)30;

// ─── Anti-debug checks ────────────────────────────────────────────────

static bool Check_PEB() {
    return IsDebuggerPresent() != FALSE;
}

static bool Check_DebugPort() {
    if (!InitNtQIP()) return false;
    DWORD_PTR port = 0;
    NTSTATUS st = pNtQIP(GetCurrentProcess(), kDebugPort, &port, sizeof(port), nullptr);
    return NT_SUCCESS(st) && port != 0;
}

static bool Check_DebugObject() {
    if (!InitNtQIP()) return false;
    HANDLE hDbg = nullptr;
    NTSTATUS st = pNtQIP(GetCurrentProcess(), kDebugObjHandle, &hDbg, sizeof(hDbg), nullptr);
    return NT_SUCCESS(st) && hDbg != nullptr;
}

static bool Check_RemoteDebugger() {
    if (!dynamic_api::pCheckRemoteDebuggerPresent) return false;
    BOOL present = FALSE;
    dynamic_api::pCheckRemoteDebuggerPresent(GetCurrentProcess(), &present);
    return present != FALSE;
}

static bool Check_HWBreakpoints() {
    if (!dynamic_api::pGetThreadContext) return false;
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!dynamic_api::pGetThreadContext(GetCurrentThread(), &ctx)) return false;
    return (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3);
}

static bool Check_HeapFlags() {
#ifdef _WIN64
    const BYTE* pPEB = (const BYTE*)__readgsqword(0x60);
    const BYTE* pHeap = *(const BYTE**)(pPEB + 0x30);
    uint32_t flags = *(const uint32_t*)(pHeap + 0x70);
    uint32_t forceFlags = *(const uint32_t*)(pHeap + 0x74);
#else
    const BYTE* pPEB = (const BYTE*)__readfsdword(0x30);
    const BYTE* pHeap = *(const BYTE**)(pPEB + 0x18);
    uint32_t flags = *(const uint32_t*)(pHeap + 0x40);
    uint32_t forceFlags = *(const uint32_t*)(pHeap + 0x44);
#endif
    return (flags & ~0x2) != 0 || forceFlags != 0;
}

static bool Check_Timing() {
    uint64_t t1 = __rdtsc();
    volatile int x = 0;
    for (int i = 0; i < 1000; ++i) x += i;
    uint64_t t2 = __rdtsc();
    return (t2 - t1) > 50'000'000ULL;
}

bool AnyDebuggerDetected() {
    return Check_PEB()
        || Check_DebugPort()
        || Check_DebugObject()
        || Check_RemoteDebugger()
        || Check_HWBreakpoints()
        || Check_HeapFlags()
        || Check_Timing();
}

[[noreturn]] void KillSelf() {
    SecureZeroMemory(&g_AuthHash, sizeof(g_AuthHash));
    TerminateProcess(GetCurrentProcess(), 0xDEAD);
    __assume(0);
}

void AntiDebugThread() {
    while (true) {
        if (AnyDebuggerDetected()) KillSelf();
        DWORD base = 200;
        LARGE_INTEGER t; QueryPerformanceCounter(&t);
        DWORD jitter = static_cast<DWORD>(t.QuadPart & 0x7F);
        std::this_thread::sleep_for(std::chrono::milliseconds(base + jitter));
    }
}

void InitAntiDebug() {
    InitNtQIP();
}