#pragma once

#include <windows.h>

// Dynamic API pointers – all functions used across the codebase.
namespace dynamic_api {

// user32
typedef UINT (WINAPI *SendInput_t)(UINT cInputs, LPINPUT pInputs, int cbSize);
typedef SHORT (WINAPI *GetAsyncKeyState_t)(int vKey);

extern SendInput_t pSendInput;
extern GetAsyncKeyState_t pGetAsyncKeyState;

// kernel32
typedef HANDLE (WINAPI *GetCurrentProcess_t)();
typedef BOOL (WINAPI *SetThreadPriority_t)(HANDLE hThread, int nPriority);
typedef BOOL (WINAPI *SetPriorityClass_t)(HANDLE hProcess, DWORD dwPriorityClass);
typedef HMODULE (WINAPI *GetModuleHandleA_t)(LPCSTR lpModuleName);
typedef FARPROC (WINAPI *GetProcAddress_t)(HMODULE hModule, LPCSTR lpProcName);
typedef BOOL (WINAPI *GetThreadContext_t)(HANDLE hThread, LPCONTEXT lpContext);
typedef BOOL (WINAPI *CheckRemoteDebuggerPresent_t)(HANDLE hProcess, PBOOL pbDebuggerPresent);
typedef VOID (WINAPI *GetSystemInfo_t)(LPSYSTEM_INFO lpSystemInfo);
typedef BOOL (WINAPI *VirtualProtect_t)(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);

extern GetCurrentProcess_t pGetCurrentProcess;
extern SetThreadPriority_t pSetThreadPriority;
extern SetPriorityClass_t pSetPriorityClass;
extern GetModuleHandleA_t pGetModuleHandleA;
extern GetProcAddress_t pGetProcAddress;
extern GetThreadContext_t pGetThreadContext;
extern CheckRemoteDebuggerPresent_t pCheckRemoteDebuggerPresent;
extern GetSystemInfo_t pGetSystemInfo;
extern VirtualProtect_t pVirtualProtect;

// advapi32 (registry)
typedef LSTATUS (WINAPI *RegOpenKeyExA_t)(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
typedef LSTATUS (WINAPI *RegQueryValueExA_t)(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
typedef LSTATUS (WINAPI *RegCloseKey_t)(HKEY hKey);

extern RegOpenKeyExA_t pRegOpenKeyExA;
extern RegQueryValueExA_t pRegQueryValueExA;
extern RegCloseKey_t pRegCloseKey;

// Initialization – returns true if critical functions are available.
bool Initialize();

} // namespace dynamic_api