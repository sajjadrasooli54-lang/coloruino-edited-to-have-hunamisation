#include "LicenseManager.h"
#include "core/ConfigManager.h"
#include "core/Config.h"
#include "security/Auth.h"
#include "util/DynamicApi.h"

#include <conio.h>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <windows.h>
#include <intrin.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "rpcrt4.lib")

namespace {
    constexpr uint64_t kFnvOff = 14695981039346656037ULL;
    constexpr uint64_t kFnvPri = 1099511628211ULL;

    constexpr uint64_t ct_fnv1a(const char* s, size_t n) {
        uint64_t h = kFnvOff;
        for (size_t i = 0; i < n; ++i)
            h = (h ^ static_cast<uint8_t>(s[i])) * kFnvPri;
        return h;
    }

    // PLACEHOLDER – replace with your 32-char lowercase hex license
    static constexpr uint64_t VALID_KEY_HASH =
        ct_fnv1a("00000000000000000000000000000000", 32);
}

std::string LicenseManager::generateHWID() {
    std::stringstream hwid;

    int cpuInfo[4] = { 0 };
    __cpuid(cpuInfo, 0);
    hwid << std::hex << cpuInfo[1] << cpuInfo[3] << cpuInfo[2];

    HKEY hKey = nullptr;
    char buffer[256] = { 0 };
    DWORD bufferSize = sizeof(buffer);

    // Use dynamic registry APIs
    if (dynamic_api::pRegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        dynamic_api::pRegQueryValueExA(hKey, "SystemManufacturer", NULL, NULL, (LPBYTE)buffer, &bufferSize);
        hwid << buffer;

        bufferSize = sizeof(buffer);
        memset(buffer, 0, sizeof(buffer));
        dynamic_api::pRegQueryValueExA(hKey, "SystemProductName", NULL, NULL, (LPBYTE)buffer, &bufferSize);
        hwid << buffer;

        dynamic_api::pRegCloseKey(hKey);
    }

    IP_ADAPTER_INFO adapterInfo[16];
    DWORD bufLen = sizeof(adapterInfo);
    DWORD status = GetAdaptersInfo(adapterInfo, &bufLen); // Leave static, it's a one-time call.

    if (status == ERROR_SUCCESS) {
        for (int i = 0; i < 6; i++) {
            hwid << std::hex << std::setfill('0') << std::setw(2)
                << (int)adapterInfo[0].Address[i];
        }
    }

    if (dynamic_api::pRegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        bufferSize = sizeof(buffer);
        memset(buffer, 0, sizeof(buffer));
        dynamic_api::pRegQueryValueExA(hKey, "InstallDate", NULL, NULL, (LPBYTE)buffer, &bufferSize);
        hwid << buffer;
        dynamic_api::pRegCloseKey(hKey);
    }

    // PLACEHOLDER 24-hex salt – replace via rotate_secrets.py
    std::string salt = "000000000000000000000000";
    hwid << salt;
    SecureZeroMemory(&salt[0], salt.size());
    SecureZeroMemory(buffer, sizeof(buffer));

    return hwid.str();
}

std::string LicenseManager::hashHWID(const std::string& hwid) {
    // PLACEHOLDER 18-char hash key
    std::string key = "PLACEHOLDER_KEY_18";
    std::string result = hwid;
    const size_t keyLength = key.size();

    for (int round = 0; round < 3; round++) {
        for (size_t i = 0; i < result.length(); i++) {
            result[i] = result[i] ^ key[(i + round) % keyLength] ^ (round + 1);
        }
    }

    SecureZeroMemory(&key[0], key.size());

    std::stringstream hexStream;
    for (unsigned char c : result) {
        hexStream << std::hex << std::setfill('0') << std::setw(2) << (int)c;
    }

    SecureZeroMemory(&result[0], result.size());
    return hexStream.str();
}

bool LicenseManager::isLicensed() {
    std::string ip;
    int port;
    if (!ConfigManager::loadIPAndPort(ip, port)) {
        return false;
    }

    std::string currentHWID = hashHWID(generateHWID());
    std::string storedHWID = ConfigManager::getLicenseFromConfig();

    bool result = !storedHWID.empty() && (currentHWID == storedHWID);

    SecureZeroMemory(&currentHWID[0], currentHWID.size());
    SecureZeroMemory(&storedHWID[0], storedHWID.size());

    return result;
}

bool LicenseManager::validateAndBindLicense(const std::string& licenseKey) {
    uint64_t inputHash = fnv1a64(licenseKey.c_str(), licenseKey.size());
    if (inputHash != VALID_KEY_HASH) {
        return false;
    }

    std::string hwid = generateHWID();
    std::string hashedHWID = hashHWID(hwid);
    SecureZeroMemory(&hwid[0], hwid.size());

    bool result = ConfigManager::saveLicenseToConfig(hashedHWID);
    SecureZeroMemory(&hashedHWID[0], hashedHWID.size());

    return result;
}

std::string LicenseManager::getHiddenInput() {
    std::string input;
    char ch;

    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hConsole, &mode);
    SetConsoleMode(hConsole, mode & ~ENABLE_ECHO_INPUT);

    while (true) {
        ch = _getch();
        if (ch == '\r' || ch == '\n') break;
        else if (ch == '\b' && !input.empty()) {
            input.pop_back();
            std::cout << "\b \b";
        }
        else if (ch >= 32 && ch <= 126) {
            input += ch;
            std::cout << '*';
        }
    }

    SetConsoleMode(hConsole, mode);
    std::cout << std::endl;
    return input;
}

bool LicenseManager::promptForLicense() {
    std::string inputKey;
    std::cout << "=== LICENSE ACTIVATION ===" << std::endl;
    std::cout << "This application requires a valid license key." << std::endl;
    std::cout << "Please enter your license key: ";

    inputKey = getHiddenInput();
    std::cout << std::endl;

    if (validateAndBindLicense(inputKey)) {
        SecureZeroMemory(&inputKey[0], inputKey.size());
        std::cout << "License activated successfully!" << std::endl;
        std::cout << "Application is now bound to this hardware." << std::endl;
        return true;
    }
    else {
        SecureZeroMemory(&inputKey[0], inputKey.size());
        std::cout << "Invalid license key!" << std::endl;
        std::cout << "Application will now exit." << std::endl;
        return false;
    }
}

bool LicenseManager::checkLicense() {
    return isLicensed();
}