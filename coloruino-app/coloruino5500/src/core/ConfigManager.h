#pragma once

#include <string>

class ConfigManager {
private:
    static std::string encryptDecrypt(const std::string& data);
    static std::string GetConfigDirectory();
    static std::string GetConfigFilePath();
    static std::string GetAuthFilePath();
    static std::string GetLogFilePath();

public:
    static bool saveConfig(const std::string& ip, int port);
    static bool saveConfig();
    static bool loadConfig();
    static bool loadIPAndPort(std::string& ip, int& port);   // [ADDED] single declaration
    static std::string getLicenseFromConfig();
    static bool saveLicenseToConfig(const std::string& hashedHWID);

    static void rotateLog(const std::string& logMessage);
};