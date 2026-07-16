#include "DynamicApi.h"
#include <iostream>
#include <cstdio>

namespace dynamic_api {

// Define pointers.
SendInput_t pSendInput = nullptr;
GetAsyncKeyState_t pGetAsyncKeyState = nullptr;

GetCurrentProcess_t pGetCurrentProcess = nullptr;
SetThreadPriority_t pSetThreadPriority = nullptr;
SetPriorityClass_t pSetPriorityClass = nullptr;
GetModuleHandleA_t pGetModuleHandleA = nullptr;
GetProcAddress_t pGetProcAddress = nullptr;
GetThreadContext_t pGetThreadContext = nullptr;
CheckRemoteDebuggerPresent_t pCheckRemoteDebuggerPresent = nullptr;
GetSystemInfo_t pGetSystemInfo = nullptr;
VirtualProtect_t pVirtualProtect = nullptr;

RegOpenKeyExA_t pRegOpenKeyExA = nullptr;
RegQueryValueExA_t pRegQueryValueExA = nullptr;
RegCloseKey_t pRegCloseKey = nullptr;

static void Log(const char* msg) {
    std::cout << "[DynamicAPI] " << msg << std::endl;
    OutputDebugStringA(msg);
}

bool Initialize() {
    Log("Initializing dynamic API...");

    HMODULE hUser32 = LoadLibraryA("user32.dll");
    HMODULE hKernel32 = LoadLibraryA("kernel32.dll");
    HMODULE hAdvapi32 = LoadLibraryA("advapi32.dll");

    bool success = true;

    if (hUser32) {
        pSendInput = (SendInput_t)GetProcAddress(hUser32, "SendInput");
        pGetAsyncKeyState = (GetAsyncKeyState_t)GetProcAddress(hUser32, "GetAsyncKeyState");
        if (pSendInput) Log("  SendInput: OK"); else { Log("  SendInput: FAILED"); success = false; }
        if (pGetAsyncKeyState) Log("  GetAsyncKeyState: OK"); else { Log("  GetAsyncKeyState: FAILED"); success = false; }
    } else {
        Log("  user32.dll: FAILED to load");
        success = false;
    }

    if (hKernel32) {
        pGetCurrentProcess = (GetCurrentProcess_t)GetProcAddress(hKernel32, "GetCurrentProcess");
        pSetThreadPriority = (SetThreadPriority_t)GetProcAddress(hKernel32, "SetThreadPriority");
        pSetPriorityClass = (SetPriorityClass_t)GetProcAddress(hKernel32, "SetPriorityClass");
        pGetModuleHandleA = (GetModuleHandleA_t)GetProcAddress(hKernel32, "GetModuleHandleA");
        pGetProcAddress = (GetProcAddress_t)GetProcAddress(hKernel32, "GetProcAddress");
        pGetThreadContext = (GetThreadContext_t)GetProcAddress(hKernel32, "GetThreadContext");
        pCheckRemoteDebuggerPresent = (CheckRemoteDebuggerPresent_t)GetProcAddress(hKernel32, "CheckRemoteDebuggerPresent");
        pGetSystemInfo = (GetSystemInfo_t)GetProcAddress(hKernel32, "GetSystemInfo");
        pVirtualProtect = (VirtualProtect_t)GetProcAddress(hKernel32, "VirtualProtect");

        if (pGetCurrentProcess) Log("  GetCurrentProcess: OK"); else Log("  GetCurrentProcess: FAILED");
        if (pSetThreadPriority) Log("  SetThreadPriority: OK"); else Log("  SetThreadPriority: FAILED");
        if (pSetPriorityClass) Log("  SetPriorityClass: OK"); else Log("  SetPriorityClass: FAILED");
        if (pGetModuleHandleA) Log("  GetModuleHandleA: OK"); else Log("  GetModuleHandleA: FAILED");
        if (pGetProcAddress) Log("  GetProcAddress: OK"); else Log("  GetProcAddress: FAILED");
        if (pGetThreadContext) Log("  GetThreadContext: OK"); else Log("  GetThreadContext: FAILED");
        if (pCheckRemoteDebuggerPresent) Log("  CheckRemoteDebuggerPresent: OK"); else Log("  CheckRemoteDebuggerPresent: FAILED");
        if (pGetSystemInfo) Log("  GetSystemInfo: OK"); else Log("  GetSystemInfo: FAILED");
        if (pVirtualProtect) Log("  VirtualProtect: OK"); else Log("  VirtualProtect: FAILED");
    } else {
        Log("  kernel32.dll: FAILED to load");
        success = false;
    }

    if (hAdvapi32) {
        pRegOpenKeyExA = (RegOpenKeyExA_t)GetProcAddress(hAdvapi32, "RegOpenKeyExA");
        pRegQueryValueExA = (RegQueryValueExA_t)GetProcAddress(hAdvapi32, "RegQueryValueExA");
        pRegCloseKey = (RegCloseKey_t)GetProcAddress(hAdvapi32, "RegCloseKey");
        if (pRegOpenKeyExA) Log("  RegOpenKeyExA: OK"); else Log("  RegOpenKeyExA: FAILED");
        if (pRegQueryValueExA) Log("  RegQueryValueExA: OK"); else Log("  RegQueryValueExA: FAILED");
        if (pRegCloseKey) Log("  RegCloseKey: OK"); else Log("  RegCloseKey: FAILED");
    } else {
        Log("  advapi32.dll: FAILED to load");
        // Not critical – we'll still try to continue.
    }

    Log(success ? "Dynamic API init SUCCESS (critical functions OK)" : "Dynamic API init FAILED (critical functions missing)");
    return success;
}

} // namespace dynamic_api