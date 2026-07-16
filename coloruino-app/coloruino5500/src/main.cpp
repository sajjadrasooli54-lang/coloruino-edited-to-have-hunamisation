#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <windows.h>
#include <signal.h>
#include <Psapi.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <cstdio>

#include "core/Config.h"
#include "core/Globals.h"
#include "core/ConfigManager.h"
#include "security/LicenseManager.h"
#include "input/HardwareInjector.h"
#include "input/IMouseInjector.h"
#include "network/WebServer.h"
#include "capture/CaptureLoop.h"
#include "capture/ColorDetector.h"
#include "input/MouseMove.h"
#include "util/SystemUtils.h"
#include "util/DynamicApi.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

std::atomic<bool> g_shutdown_requested{ false };
std::condition_variable g_shutdown_cv;
std::mutex g_shutdown_mutex;

std::unique_ptr<IMouseInjector> g_injector;  // global injector pointer

void requestShutdown() {
    g_shutdown_requested = true;
    g_shutdown_cv.notify_one();
}

static std::mt19937 mt{ std::random_device{}() };

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
        ConfigManager::rotateLog("Shutting down cleanly...");
        requestShutdown();
        return TRUE;
    }
    return FALSE;
}

int get_random_int(int min, int max) {
    std::uniform_int_distribution<int> dist{ min, max };
    return dist(mt);
}

static int ComputeFreshTimeoutMs() {
    int refresh = MonitorInfo::GetRefreshRate();
    if (refresh <= 0) refresh = 60;
    int t = (2 * 1000 / refresh) + 2;
    if (t < 6) t = 6;
    return t;
}

static bool WaitForFreshCapture(uint64_t seqBefore, int timeoutMs,
    const int& coordsX, const int& coordsY,
    int& outX, int& outY)
{
    auto start = std::chrono::high_resolution_clock::now();
    while (true) {
        uint64_t curSeq = capture_seq.load(std::memory_order_acquire);
        if (curSeq != seqBefore) {
            int x = coordsX;
            int y = coordsY;
            if (x != 0 || y != 0) {
                outX = x;
                outY = y;
                return true;
            }
            seqBefore = curSeq;
        }
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= timeoutMs) {
            outX = 0;
            outY = 0;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

static bool IsGameForeground() { return true; }

void mode_a() {
    if (dynamic_api::pSetThreadPriority)
        dynamic_api::pSetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    bool key_was_down = false;
    auto lastFireTime = std::chrono::high_resolution_clock::now();
    constexpr auto DEBOUNCE = std::chrono::milliseconds(20);
    const int FRESH_TIMEOUT_MS = ComputeFreshTimeoutMs();

    while (true) {
        if (cfg::mode_a_ativo) {
            bool key_is_down = false;
            if (dynamic_api::pGetAsyncKeyState)
                key_is_down = dynamic_api::pGetAsyncKeyState(cfg::mode_a_key) & 0x8000;
            else
                key_is_down = GetAsyncKeyState(cfg::mode_a_key) & 0x8000;
            auto now = std::chrono::high_resolution_clock::now();
            if (key_is_down && !key_was_down && (now - lastFireTime) > DEBOUNCE) {
                if (!IsGameForeground()) {
                    key_was_down = key_is_down;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                uint64_t seqBefore = capture_seq.load(std::memory_order_acquire);
                int mx = mode_a_x;
                int my = mode_a_y;
                if (mx == 0 && my == 0) {
                    WaitForFreshCapture(seqBefore, FRESH_TIMEOUT_MS, mode_a_x, mode_a_y, mx, my);
                    if (mx == 0 && my == 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        mx = mode_a_x;
                        my = mode_a_y;
                    }
                }
                if (mx != 0 || my != 0) {
                    SnapShoot_P(mx, my);
                    lastFireTime = std::chrono::high_resolution_clock::now();
                }
            }
            key_was_down = key_is_down;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void nonmode_a() {
    if (dynamic_api::pSetThreadPriority)
        dynamic_api::pSetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    bool key_was_down = false;
    auto lastFireTime = std::chrono::high_resolution_clock::now();
    constexpr auto DEBOUNCE = std::chrono::milliseconds(20);
    const int FRESH_TIMEOUT_MS = ComputeFreshTimeoutMs();

    while (true) {
        if (cfg::nonmode_a_ativo) {
            bool key_is_down = false;
            if (dynamic_api::pGetAsyncKeyState)
                key_is_down = dynamic_api::pGetAsyncKeyState(cfg::nonmode_a_key) & 0x8000;
            else
                key_is_down = GetAsyncKeyState(cfg::nonmode_a_key) & 0x8000;
            auto now = std::chrono::high_resolution_clock::now();
            if (key_is_down && !key_was_down && (now - lastFireTime) > DEBOUNCE) {
                if (!IsGameForeground()) {
                    key_was_down = key_is_down;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                uint64_t seqBefore = capture_seq.load(std::memory_order_acquire);
                int mx = nonmode_a_x;
                int my = nonmode_a_y;
                if (mx == 0 && my == 0) {
                    WaitForFreshCapture(seqBefore, FRESH_TIMEOUT_MS, nonmode_a_x, nonmode_a_y, mx, my);
                    if (mx == 0 && my == 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        mx = nonmode_a_x;
                        my = nonmode_a_y;
                    }
                }
                if (mx != 0 || my != 0) {
                    SnapShoot_F(mx, my);
                    lastFireTime = std::chrono::high_resolution_clock::now();
                }
            }
            key_was_down = key_is_down;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool IsAdmin() {
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdminGroup;
    BOOL bResult = AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdminGroup);
    if (!bResult) return false;
    BOOL isAdmin = FALSE;
    bResult = CheckTokenMembership(NULL, AdminGroup, &isAdmin);
    FreeSid(AdminGroup);
    return bResult && isAdmin;
}

static void InitFOV() {
    int best = 0;
    if (cfg::apply_delta_ativo && cfg::apply_delta_fov > best) best = cfg::apply_delta_fov;
    if (cfg::apply_deltaassist_ativo && cfg::apply_deltaassist_fov > best) best = cfg::apply_deltaassist_fov;
    if (cfg::mode_a_ativo && cfg::mode_a_fov > best) best = cfg::mode_a_fov;
    if (cfg::nonmode_a_ativo && cfg::nonmode_a_fov > best) best = cfg::nonmode_a_fov;
    if (best <= 0) best = 200;
    std::lock_guard<std::mutex> lock(fovMutex);
    currentFOV = best;
}

static void OpenWebUIInEdge() {
    const char* url = "http://localhost:13548/";
    char edgePath[MAX_PATH] = {0};
    HKEY hKey;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = REG_SZ;
        DWORD pathLen = sizeof(edgePath);
        if (RegQueryValueExA(hKey, NULL, NULL, &type, (LPBYTE)edgePath, &pathLen) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            std::string cmd = std::string("\"") + edgePath + "\" --new-window --app=" + url;
            STARTUPINFOA si = {};
            PROCESS_INFORMATION pi = {};
            si.cb = sizeof(si);
            if (CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, FALSE,
                               CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                return;
            }
        } else {
            RegCloseKey(hKey);
        }
    }

    const char* fallbackPaths[] = {
        "C:\\Program Files (x86)\\Microsoft Edge\\Application\\msedge.exe",
        "C:\\Program Files\\Microsoft Edge\\Application\\msedge.exe"
    };
    for (const char* path : fallbackPaths) {
        if (PathFileExistsA(path)) {
            std::string cmd = std::string("\"") + path + "\" --new-window --app=" + url;
            STARTUPINFOA si = {};
            PROCESS_INFORMATION pi = {};
            si.cb = sizeof(si);
            if (CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, FALSE,
                               CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                return;
            }
        }
    }

    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

int main() {
    try {
        AllocConsole();
        FILE* fDummy;
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
        std::cout << "[DEBUG] Console attached." << std::endl;

        std::cout << "[DEBUG] Initializing dynamic API..." << std::endl;
        bool apiInitSuccess = dynamic_api::Initialize();
        std::cout << "[DEBUG] Dynamic API init result: " << (apiInitSuccess ? "SUCCESS" : "FAILED") << std::endl;

        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
        ConfigManager::rotateLog("Coloruino started.");
        std::cout << "[DEBUG] Coloruino started." << std::endl;

        if (!apiInitSuccess) {
            ConfigManager::rotateLog("WARNING: Dynamic API initialization partially failed. Some functions may not be available.");
            std::cout << "[WARN] Dynamic API initialization partially failed." << std::endl;
        }

        if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            SetProcessDPIAware();
            std::cout << "[DEBUG] Set DPI awareness fallback." << std::endl;
        }

        std::cout << "[DEBUG] Loading config..." << std::endl;
        if (!ConfigManager::loadConfig()) {
            std::cout << "[WARN] Config load failed or file missing, using defaults." << std::endl;
        } else {
            std::cout << "[DEBUG] Config loaded." << std::endl;
        }

        std::cout << "[DEBUG] Initializing HardwareInjector..." << std::endl;
        auto hardwareInjector = std::make_unique<HardwareInjector>();
        if (hardwareInjector->Connect()) {
            g_injector = std::move(hardwareInjector);
            std::cout << "[DEBUG] HardwareInjector connected successfully." << std::endl;
            g_injector->SetCooldown(cfg::mode_a_cooldown_ms);
        } else {
            std::cout << "[ERROR] HardwareInjector connection failed. Exiting." << std::endl;
            ConfigManager::rotateLog("ERROR: HardwareInjector connection failed.");
            std::cout << "Press Enter to exit..." << std::endl;
            std::cin.get();
            return -1;
        }

        std::cout << "[DEBUG] Setting process priority and timer resolution..." << std::endl;
        set_process_priority(HIGH_PRIORITY_CLASS);
        set_timer_resolution();
        std::cout << "[DEBUG] Done." << std::endl;

        Width = GetSystemMetrics(SM_CXSCREEN);
        Height = GetSystemMetrics(SM_CYSCREEN);
        std::cout << "[DEBUG] Screen resolution: " << Width << "x" << Height << std::endl;

        std::cout << "[DEBUG] Initializing FOV..." << std::endl;
        InitFOV();
        std::cout << "[DEBUG] FOV init complete." << std::endl;

        std::cout << "[DEBUG] Starting capture thread..." << std::endl;
        std::thread(CaptureScreen).detach();
        std::cout << "[DEBUG] Starting silent aim thread..." << std::endl;
        std::thread(mode_a).detach();
        std::cout << "[DEBUG] Starting flicker thread..." << std::endl;
        std::thread(nonmode_a).detach();
        std::cout << "[DEBUG] All threads started." << std::endl;

        std::cout << "[DEBUG] Calling initializeNetworking(13548)..." << std::endl;
        initializeNetworking(13548);
        std::cout << "[DEBUG] initializeNetworking returned." << std::endl;

        std::cout << "[DEBUG] Waiting 1 second for server to start..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "[DEBUG] Opening WebUI in Edge..." << std::endl;
        OpenWebUIInEdge();
        std::cout << "[DEBUG] WebUI open call complete." << std::endl;

        std::cout << "[DEBUG] Waiting for shutdown signal..." << std::endl;
        {
            std::unique_lock<std::mutex> lock(g_shutdown_mutex);
            g_shutdown_cv.wait(lock, [] { return g_shutdown_requested.load(); });
        }

        ConfigManager::rotateLog("Shutting down...");
        std::cout << "[DEBUG] Shutting down." << std::endl;

        // Cleanup injector
        g_injector.reset();

        std::cout << "[DEBUG] Press Enter to exit..." << std::endl;
        std::cin.get();
    }
    catch (const std::exception& e) {
        std::cout << "[FATAL] Exception: " << e.what() << std::endl;
        ConfigManager::rotateLog(std::string("FATAL: ") + e.what());
        std::cout << "[DEBUG] Press Enter to exit..." << std::endl;
        std::cin.get();
    }
    catch (...) {
        std::cout << "[FATAL] Unknown exception." << std::endl;
        ConfigManager::rotateLog("FATAL: Unknown exception");
        std::cout << "[DEBUG] Press Enter to exit..." << std::endl;
        std::cin.get();
    }
    return 0;
}