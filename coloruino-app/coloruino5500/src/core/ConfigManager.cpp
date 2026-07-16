#include "ConfigManager.h"
#include "Config.h"
#include "xorstr.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <Windows.h>
#include <filesystem>
#include <shlobj.h>

std::string ConfigManager::GetConfigDirectory() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        std::string dir = path;
        dir += "\\AMD\\RadeonSettings\\";
        return dir;
    }
    return ".\\";
}

std::string ConfigManager::GetConfigFilePath() {
    return GetConfigDirectory() + "data";
}

std::string ConfigManager::GetAuthFilePath() {
    return GetConfigDirectory() + "auth.dat";
}

std::string ConfigManager::GetLogFilePath() {
    return GetConfigDirectory() + "diag.log";
}

void ConfigManager::rotateLog(const std::string& logMessage) {
    constexpr size_t MAX_LOG_SIZE = 5 * 1024 * 1024;
    std::string logFile = GetLogFilePath();
    std::filesystem::create_directories(GetConfigDirectory());
    if (std::filesystem::exists(logFile) &&
        std::filesystem::file_size(logFile) > MAX_LOG_SIZE) {
        std::filesystem::rename(logFile, logFile + ".old");
    }
    std::ofstream log(logFile, std::ios::app);
    if (log.is_open()) {
        log << logMessage << std::endl;
        log.close();
    }
}

std::string ConfigManager::encryptDecrypt(const std::string& data) {
    std::string key(xorstr_("000000000000000000000000"));
    std::string result = data;
    const size_t keyLength = key.size();
    for (size_t i = 0; i < result.length(); i++) {
        result[i] = result[i] ^ key[i % keyLength];
    }
    SecureZeroMemory(&key[0], key.size());
    return result;
}

bool ConfigManager::loadIPAndPort(std::string& ip, int& port) {
    try {
        std::ifstream inFile(GetConfigFilePath(), std::ios::binary);
        if (!inFile.is_open()) return false;
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        inFile.close();
        std::string decryptedData = encryptDecrypt(buffer.str());
        std::istringstream iss(decryptedData);
        std::string line;
        if (std::getline(iss, line)) {
            ip = line;
            if (std::getline(iss, line)) {
                try {
                    port = std::stoi(line);
                    SecureZeroMemory(&decryptedData[0], decryptedData.size());
                    return true;
                }
                catch (...) {
                    SecureZeroMemory(&decryptedData[0], decryptedData.size());
                    return false;
                }
            }
        }
        SecureZeroMemory(&decryptedData[0], decryptedData.size());
        return false;
    }
    catch (...) { return false; }
}

bool ConfigManager::saveConfig() {
    try {
        std::filesystem::create_directories(GetConfigDirectory());

        // Start with LICENSE_HWID line (preserve existing if any)
        std::string existingLicense = getLicenseFromConfig();
        std::stringstream configData;
        if (!existingLicense.empty()) {
            configData << xorstr_("LICENSE_HWID=") << existingLicense << "\n";
        }
        configData << xorstr_("---CONFIG_START---") << "\n";

        #define WRITE_BOOL(name) configData << #name "=" << (cfg::name ? "1" : "0") << "\n"
        #define WRITE_INT(name) configData << #name "=" << cfg::name << "\n"
        #define WRITE_FLOAT(name) configData << #name "=" << cfg::name << "\n"

        // Aimbot
        WRITE_BOOL(apply_delta_ativo);
        WRITE_INT(apply_deltakey1);
        WRITE_INT(apply_deltakey2);
        WRITE_INT(target_offset_x);
        WRITE_INT(target_offset_y);
        WRITE_INT(apply_delta_fov);
        WRITE_FLOAT(apply_delta_smooth);
        WRITE_FLOAT(speed);
        WRITE_INT(sleep);
        WRITE_BOOL(apply_delta_toggle);

        // Assist
        WRITE_BOOL(apply_deltaassist_ativo);
        WRITE_INT(assist_apply_deltakey);
        WRITE_INT(assist_target_offset_x);
        WRITE_INT(assist_target_offset_y);
        WRITE_INT(apply_deltaassist_fov);
        WRITE_FLOAT(apply_deltaassist_smooth);
        WRITE_FLOAT(assist_speed);
        WRITE_INT(assist_sleep);

        // Flicker
        WRITE_BOOL(nonmode_a_ativo);
        WRITE_INT(nonmode_a_key);
        WRITE_INT(nonmode_a_fov);
        WRITE_INT(nonmode_a_delay_between_shots);
        WRITE_FLOAT(nonmode_a_distance);

        // Silent
        WRITE_BOOL(mode_a_ativo);
        WRITE_INT(mode_a_key);
        WRITE_INT(mode_a_target_offset_x);
        WRITE_INT(mode_a_target_offset_y);
        WRITE_INT(mode_a_fov);
        WRITE_INT(mode_a_delay_between_shots);
        WRITE_FLOAT(distance);
        WRITE_BOOL(mode_a_head_targeting);
        WRITE_INT(mode_a_cooldown_ms);

        // Trigger
        WRITE_BOOL(trigger_action_ativo);
        WRITE_INT(trigger_action_key);
        WRITE_INT(trigger_action_delay);
        WRITE_INT(trigger_action_fovX);
        WRITE_INT(trigger_action_fovY);

        // Color / Filter
        WRITE_BOOL(useIstrigFilter);
        WRITE_INT(color_mode);
        for (int i = 0; i < 3; i++) {
            configData << "menorRGB" << i << "=" << cfg::menorRGB[i] << "\n";
            configData << "maiorRGB" << i << "=" << cfg::maiorRGB[i] << "\n";
            configData << "menorHSV" << i << "=" << cfg::menorHSV[i] << "\n";
            configData << "maiorHSV" << i << "=" << cfg::maiorHSV[i] << "\n";
        }
        WRITE_INT(ngrok);
        WRITE_BOOL(use_gpu_processing);
        WRITE_BOOL(dead_body_filter);
        WRITE_INT(dead_body_threshold);
        WRITE_INT(min_cluster_size);
        WRITE_BOOL(trigger_polygon_check);
        WRITE_BOOL(apply_delta_dist_smoothing);
        WRITE_INT(apply_delta_near_dist);
        WRITE_INT(apply_delta_mid_dist);
        WRITE_FLOAT(apply_delta_near_mult);
        WRITE_FLOAT(apply_delta_mid_mult);
        WRITE_BOOL(head_anchor_proportional);
        WRITE_INT(head_anchor_band_rows);
        WRITE_INT(head_anchor_gap_tolerance);
        WRITE_INT(head_anchor_close_pct);
        WRITE_INT(head_anchor_mid_pct);
        WRITE_INT(head_anchor_close_min_h);
        WRITE_INT(head_anchor_mid_min_h);

        // Humanisation
        WRITE_BOOL(sim_enable_humanization);
        WRITE_INT(sim_jitter_min_us);
        WRITE_INT(sim_jitter_max_us);
        WRITE_INT(sim_substeps_mode);
        WRITE_INT(sim_click_hold_min_ms);
        WRITE_INT(sim_click_hold_max_ms);
        WRITE_INT(sim_snapback_delay_us);
        WRITE_FLOAT(sim_overshoot_percent);
        WRITE_INT(sim_pre_delay_min_ms);
        WRITE_INT(sim_pre_delay_max_ms);
        WRITE_INT(sim_deadzone_pixels);
        WRITE_INT(sim_easing_mode);

        // Click Behavior
        WRITE_INT(click_burst_limit);
        WRITE_INT(click_gap_base_ms);
        WRITE_INT(click_gap_jitter_ms);
        WRITE_INT(click_gap_floor_ms);
        WRITE_INT(click_scatter_percent);
        WRITE_FLOAT(click_ln_mu);
        WRITE_FLOAT(click_ln_sigma);
        WRITE_INT(click_press_min_ms);
        WRITE_INT(click_press_max_ms);
        WRITE_FLOAT(click_double_chance);
        WRITE_INT(click_double_min_ms);
        WRITE_INT(click_double_max_ms);

        // Session Drift
        WRITE_INT(drift_step_ms);
        WRITE_INT(drift_max_ms);
        WRITE_INT(drift_interval_s);
        WRITE_INT(drift_interval_jitter_s);

        // Reaction Model
        WRITE_INT(reaction_hesit_min_ms);
        WRITE_INT(reaction_hesit_max_ms);
        WRITE_INT(reaction_hesit_percent);
        WRITE_INT(reaction_cd_jitter_percent);
        WRITE_INT(reaction_refr_low);
        WRITE_INT(reaction_refr_high);
        WRITE_INT(reaction_low_dly_min);
        WRITE_INT(reaction_low_dly_max);
        WRITE_INT(reaction_hi_dly_min);
        WRITE_INT(reaction_hi_dly_max);
        WRITE_INT(reaction_reset_after_ms);

        // Hitbox Detection
        WRITE_BOOL(hitbox_detection_enabled);
        WRITE_INT(hitbox_ray_length);
        WRITE_BOOL(hitbox_use_three_rays);
        WRITE_BOOL(hitbox_anti_below);

        // Short Stop
        WRITE_BOOL(short_stop_enabled);
        WRITE_INT(short_stop_chance);
        WRITE_INT(short_stop_min_ms);
        WRITE_INT(short_stop_max_ms);
        WRITE_INT(short_stop_mode);
        WRITE_FLOAT(short_stop_slow_min);
        WRITE_FLOAT(short_stop_slow_max);

        // Track Delay
        WRITE_BOOL(track_delay_enabled);
        WRITE_INT(track_delay_min_ms);
        WRITE_INT(track_delay_max_ms);

        // AoFI
        WRITE_BOOL(aofi_enabled);
        WRITE_INT(aofi_reset_timeout_ms);

        #undef WRITE_BOOL
        #undef WRITE_INT
        #undef WRITE_FLOAT

        std::string encryptedData = encryptDecrypt(configData.str());

        std::ofstream outFile(GetConfigFilePath(), std::ios::binary | std::ios::trunc);
        if (!outFile.is_open()) {
            std::cout << "[ConfigManager] ERROR: Failed to open config file for writing: " << GetConfigFilePath() << std::endl;
            return false;
        }
        outFile.write(encryptedData.c_str(), encryptedData.size());
        outFile.close();
        std::cout << "[ConfigManager] Config saved successfully to " << GetConfigFilePath() << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cout << "[ConfigManager] Exception in saveConfig: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::loadConfig() {
    try {
        std::ifstream inFile(GetConfigFilePath(), std::ios::binary);
        if (!inFile.is_open()) {
            std::cout << "[ConfigManager] Config file not found, using defaults." << std::endl;
            return false;
        }

        std::stringstream buffer;
        buffer << inFile.rdbuf();
        inFile.close();

        std::string decryptedData = encryptDecrypt(buffer.str());

        std::istringstream iss(decryptedData);
        std::string line;

        // Read until ---CONFIG_START---
        while (std::getline(iss, line)) {
            if (line.find(xorstr_("LICENSE_HWID=")) == 0) continue;
            if (line == xorstr_("---CONFIG_START---")) break;
        }

        // ---- Parsing loop ----
        while (std::getline(iss, line)) {
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = line.substr(0, pos);
            std::string valueStr = line.substr(pos + 1);

            // Aimbot
            if (key == "apply_delta_ativo") { cfg::apply_delta_ativo = (valueStr == "1"); continue; }
            if (key == "apply_deltakey1") { cfg::apply_deltakey1 = std::stoi(valueStr); continue; }
            if (key == "apply_deltakey2") { cfg::apply_deltakey2 = std::stoi(valueStr); continue; }
            if (key == "target_offset_x") { cfg::target_offset_x = std::stoi(valueStr); continue; }
            if (key == "target_offset_y") { cfg::target_offset_y = std::stoi(valueStr); continue; }
            if (key == "apply_delta_fov") { cfg::apply_delta_fov = std::stoi(valueStr); continue; }
            if (key == "apply_delta_smooth") { cfg::apply_delta_smooth = std::stof(valueStr); continue; }
            if (key == "speed") { cfg::speed = std::stof(valueStr); continue; }
            if (key == "sleep") { cfg::sleep = std::stoi(valueStr); continue; }
            if (key == "apply_delta_toggle") { cfg::apply_delta_toggle = (valueStr == "1"); continue; }

            // Assist
            if (key == "apply_deltaassist_ativo") { cfg::apply_deltaassist_ativo = (valueStr == "1"); continue; }
            if (key == "assist_apply_deltakey") { cfg::assist_apply_deltakey = std::stoi(valueStr); continue; }
            if (key == "assist_target_offset_x") { cfg::assist_target_offset_x = std::stoi(valueStr); continue; }
            if (key == "assist_target_offset_y") { cfg::assist_target_offset_y = std::stoi(valueStr); continue; }
            if (key == "apply_deltaassist_fov") { cfg::apply_deltaassist_fov = std::stoi(valueStr); continue; }
            if (key == "apply_deltaassist_smooth") { cfg::apply_deltaassist_smooth = std::stof(valueStr); continue; }
            if (key == "assist_speed") { cfg::assist_speed = std::stof(valueStr); continue; }
            if (key == "assist_sleep") { cfg::assist_sleep = std::stoi(valueStr); continue; }

            // Flicker
            if (key == "nonmode_a_ativo") { cfg::nonmode_a_ativo = (valueStr == "1"); continue; }
            if (key == "nonmode_a_key") { cfg::nonmode_a_key = std::stoi(valueStr); continue; }
            if (key == "nonmode_a_fov") { cfg::nonmode_a_fov = std::stoi(valueStr); continue; }
            if (key == "nonmode_a_delay_between_shots") { cfg::nonmode_a_delay_between_shots = std::stoi(valueStr); continue; }
            if (key == "nonmode_a_distance") { cfg::nonmode_a_distance = std::stof(valueStr); continue; }

            // Silent
            if (key == "mode_a_ativo") { cfg::mode_a_ativo = (valueStr == "1"); continue; }
            if (key == "mode_a_key") { cfg::mode_a_key = std::stoi(valueStr); continue; }
            if (key == "mode_a_target_offset_x") { cfg::mode_a_target_offset_x = std::stoi(valueStr); continue; }
            if (key == "mode_a_target_offset_y") { cfg::mode_a_target_offset_y = std::stoi(valueStr); continue; }
            if (key == "mode_a_fov") { cfg::mode_a_fov = std::stoi(valueStr); continue; }
            if (key == "mode_a_delay_between_shots") { cfg::mode_a_delay_between_shots = std::stoi(valueStr); continue; }
            if (key == "distance") { cfg::distance = std::stof(valueStr); continue; }
            if (key == "mode_a_head_targeting") { cfg::mode_a_head_targeting = (valueStr == "1"); continue; }
            if (key == "mode_a_cooldown_ms") { cfg::mode_a_cooldown_ms = std::stoi(valueStr); continue; }

            // Trigger
            if (key == "trigger_action_ativo") { cfg::trigger_action_ativo = (valueStr == "1"); continue; }
            if (key == "trigger_action_key") { cfg::trigger_action_key = std::stoi(valueStr); continue; }
            if (key == "trigger_action_delay") { cfg::trigger_action_delay = std::stoi(valueStr); continue; }
            if (key == "trigger_action_fovX") { cfg::trigger_action_fovX = std::stoi(valueStr); continue; }
            if (key == "trigger_action_fovY") { cfg::trigger_action_fovY = std::stoi(valueStr); continue; }

            // Color / Filter
            if (key == "useIstrigFilter") { cfg::useIstrigFilter = (valueStr == "1"); continue; }
            if (key == "color_mode") { cfg::color_mode = std::stoi(valueStr); continue; }
            if (key == "ngrok") { cfg::ngrok = std::stoi(valueStr); continue; }
            if (key == "use_gpu_processing") { cfg::use_gpu_processing = (valueStr == "1"); continue; }
            if (key == "dead_body_filter") { cfg::dead_body_filter = (valueStr == "1"); continue; }
            if (key == "dead_body_threshold") { cfg::dead_body_threshold = std::stoi(valueStr); continue; }
            if (key == "min_cluster_size") { cfg::min_cluster_size = std::stoi(valueStr); continue; }
            if (key == "trigger_polygon_check") { cfg::trigger_polygon_check = (valueStr == "1"); continue; }
            if (key == "apply_delta_dist_smoothing") { cfg::apply_delta_dist_smoothing = (valueStr == "1"); continue; }
            if (key == "apply_delta_near_dist") { cfg::apply_delta_near_dist = std::stoi(valueStr); continue; }
            if (key == "apply_delta_mid_dist") { cfg::apply_delta_mid_dist = std::stoi(valueStr); continue; }
            if (key == "apply_delta_near_mult") { cfg::apply_delta_near_mult = std::stof(valueStr); continue; }
            if (key == "apply_delta_mid_mult") { cfg::apply_delta_mid_mult = std::stof(valueStr); continue; }
            if (key == "head_anchor_proportional") { cfg::head_anchor_proportional = (valueStr == "1"); continue; }
            if (key == "head_anchor_band_rows") { cfg::head_anchor_band_rows = std::stoi(valueStr); continue; }
            if (key == "head_anchor_gap_tolerance") { cfg::head_anchor_gap_tolerance = std::stoi(valueStr); continue; }
            if (key == "head_anchor_close_pct") { cfg::head_anchor_close_pct = std::stoi(valueStr); continue; }
            if (key == "head_anchor_mid_pct") { cfg::head_anchor_mid_pct = std::stoi(valueStr); continue; }
            if (key == "head_anchor_close_min_h") { cfg::head_anchor_close_min_h = std::stoi(valueStr); continue; }
            if (key == "head_anchor_mid_min_h") { cfg::head_anchor_mid_min_h = std::stoi(valueStr); continue; }

            // Humanisation
            if (key == "sim_enable_humanization") { cfg::sim_enable_humanization = (valueStr == "1"); continue; }
            if (key == "sim_jitter_min_us") { cfg::sim_jitter_min_us = std::stoi(valueStr); continue; }
            if (key == "sim_jitter_max_us") { cfg::sim_jitter_max_us = std::stoi(valueStr); continue; }
            if (key == "sim_substeps_mode") { cfg::sim_substeps_mode = std::stoi(valueStr); continue; }
            if (key == "sim_click_hold_min_ms") { cfg::sim_click_hold_min_ms = std::stoi(valueStr); continue; }
            if (key == "sim_click_hold_max_ms") { cfg::sim_click_hold_max_ms = std::stoi(valueStr); continue; }
            if (key == "sim_snapback_delay_us") { cfg::sim_snapback_delay_us = std::stoi(valueStr); continue; }
            if (key == "sim_overshoot_percent") { cfg::sim_overshoot_percent = std::stof(valueStr); continue; }
            if (key == "sim_pre_delay_min_ms") { cfg::sim_pre_delay_min_ms = std::stoi(valueStr); continue; }
            if (key == "sim_pre_delay_max_ms") { cfg::sim_pre_delay_max_ms = std::stoi(valueStr); continue; }
            if (key == "sim_deadzone_pixels") { cfg::sim_deadzone_pixels = std::stoi(valueStr); continue; }
            if (key == "sim_easing_mode") { cfg::sim_easing_mode = std::stoi(valueStr); continue; }

            // Click Behavior
            if (key == "click_burst_limit") { cfg::click_burst_limit = std::stoi(valueStr); continue; }
            if (key == "click_gap_base_ms") { cfg::click_gap_base_ms = std::stoi(valueStr); continue; }
            if (key == "click_gap_jitter_ms") { cfg::click_gap_jitter_ms = std::stoi(valueStr); continue; }
            if (key == "click_gap_floor_ms") { cfg::click_gap_floor_ms = std::stoi(valueStr); continue; }
            if (key == "click_scatter_percent") { cfg::click_scatter_percent = std::stoi(valueStr); continue; }
            if (key == "click_ln_mu") { cfg::click_ln_mu = std::stof(valueStr); continue; }
            if (key == "click_ln_sigma") { cfg::click_ln_sigma = std::stof(valueStr); continue; }
            if (key == "click_press_min_ms") { cfg::click_press_min_ms = std::stoi(valueStr); continue; }
            if (key == "click_press_max_ms") { cfg::click_press_max_ms = std::stoi(valueStr); continue; }
            if (key == "click_double_chance") { cfg::click_double_chance = std::stof(valueStr); continue; }
            if (key == "click_double_min_ms") { cfg::click_double_min_ms = std::stoi(valueStr); continue; }
            if (key == "click_double_max_ms") { cfg::click_double_max_ms = std::stoi(valueStr); continue; }

            // Session Drift
            if (key == "drift_step_ms") { cfg::drift_step_ms = std::stoi(valueStr); continue; }
            if (key == "drift_max_ms") { cfg::drift_max_ms = std::stoi(valueStr); continue; }
            if (key == "drift_interval_s") { cfg::drift_interval_s = std::stoi(valueStr); continue; }
            if (key == "drift_interval_jitter_s") { cfg::drift_interval_jitter_s = std::stoi(valueStr); continue; }

            // Reaction Model
            if (key == "reaction_hesit_min_ms") { cfg::reaction_hesit_min_ms = std::stoi(valueStr); continue; }
            if (key == "reaction_hesit_max_ms") { cfg::reaction_hesit_max_ms = std::stoi(valueStr); continue; }
            if (key == "reaction_hesit_percent") { cfg::reaction_hesit_percent = std::stoi(valueStr); continue; }
            if (key == "reaction_cd_jitter_percent") { cfg::reaction_cd_jitter_percent = std::stoi(valueStr); continue; }
            if (key == "reaction_refr_low") { cfg::reaction_refr_low = std::stoi(valueStr); continue; }
            if (key == "reaction_refr_high") { cfg::reaction_refr_high = std::stoi(valueStr); continue; }
            if (key == "reaction_low_dly_min") { cfg::reaction_low_dly_min = std::stoi(valueStr); continue; }
            if (key == "reaction_low_dly_max") { cfg::reaction_low_dly_max = std::stoi(valueStr); continue; }
            if (key == "reaction_hi_dly_min") { cfg::reaction_hi_dly_min = std::stoi(valueStr); continue; }
            if (key == "reaction_hi_dly_max") { cfg::reaction_hi_dly_max = std::stoi(valueStr); continue; }
            if (key == "reaction_reset_after_ms") { cfg::reaction_reset_after_ms = std::stoi(valueStr); continue; }

            // Hitbox Detection
            if (key == "hitbox_detection_enabled") { cfg::hitbox_detection_enabled = (valueStr == "1"); continue; }
            if (key == "hitbox_ray_length") { cfg::hitbox_ray_length = std::stoi(valueStr); continue; }
            if (key == "hitbox_use_three_rays") { cfg::hitbox_use_three_rays = (valueStr == "1"); continue; }
            if (key == "hitbox_anti_below") { cfg::hitbox_anti_below = (valueStr == "1"); continue; }

            // Short Stop
            if (key == "short_stop_enabled") { cfg::short_stop_enabled = (valueStr == "1"); continue; }
            if (key == "short_stop_chance") { cfg::short_stop_chance = std::stoi(valueStr); continue; }
            if (key == "short_stop_min_ms") { cfg::short_stop_min_ms = std::stoi(valueStr); continue; }
            if (key == "short_stop_max_ms") { cfg::short_stop_max_ms = std::stoi(valueStr); continue; }
            if (key == "short_stop_mode") { cfg::short_stop_mode = std::stoi(valueStr); continue; }
            if (key == "short_stop_slow_min") { cfg::short_stop_slow_min = std::stof(valueStr); continue; }
            if (key == "short_stop_slow_max") { cfg::short_stop_slow_max = std::stof(valueStr); continue; }

            // Track Delay
            if (key == "track_delay_enabled") { cfg::track_delay_enabled = (valueStr == "1"); continue; }
            if (key == "track_delay_min_ms") { cfg::track_delay_min_ms = std::stoi(valueStr); continue; }
            if (key == "track_delay_max_ms") { cfg::track_delay_max_ms = std::stoi(valueStr); continue; }

            // AoFI
            if (key == "aofi_enabled") { cfg::aofi_enabled = (valueStr == "1"); continue; }
            if (key == "aofi_reset_timeout_ms") { cfg::aofi_reset_timeout_ms = std::stoi(valueStr); continue; }

            // RGB/HSV arrays
            if (key.find("menorRGB") == 0 && key.length() == 9) {
                int index = key[8] - '0';
                if (index >= 0 && index < 3) cfg::menorRGB[index] = std::stoi(valueStr);
                continue;
            }
            if (key.find("maiorRGB") == 0 && key.length() == 9) {
                int index = key[8] - '0';
                if (index >= 0 && index < 3) cfg::maiorRGB[index] = std::stoi(valueStr);
                continue;
            }
            if (key.find("menorHSV") == 0 && key.length() == 9) {
                int index = key[8] - '0';
                if (index >= 0 && index < 3) cfg::menorHSV[index] = std::stoi(valueStr);
                continue;
            }
            if (key.find("maiorHSV") == 0 && key.length() == 9) {
                int index = key[8] - '0';
                if (index >= 0 && index < 3) cfg::maiorHSV[index] = std::stoi(valueStr);
                continue;
            }
        }

        SecureZeroMemory(&decryptedData[0], decryptedData.size());
        std::cout << "[ConfigManager] Config loaded successfully from " << GetConfigFilePath() << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cout << "[ConfigManager] Exception in loadConfig: " << e.what() << std::endl;
        return false;
    }
}

std::string ConfigManager::getLicenseFromConfig() {
    try {
        std::ifstream inFile(GetConfigFilePath(), std::ios::binary);
        if (!inFile.is_open()) return "";
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        inFile.close();
        std::string decryptedData = encryptDecrypt(buffer.str());
        std::istringstream iss(decryptedData);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find(xorstr_("LICENSE_HWID=")) == 0) {
                std::string result = line.substr(13);
                SecureZeroMemory(&decryptedData[0], decryptedData.size());
                return result;
            }
            if (line == xorstr_("---CONFIG_START---")) break;
        }
        SecureZeroMemory(&decryptedData[0], decryptedData.size());
        return "";
    }
    catch (...) { return ""; }
}

bool ConfigManager::saveLicenseToConfig(const std::string& hashedHWID) {
    try {
        std::filesystem::create_directories(GetConfigDirectory());
        return saveConfig();
    }
    catch (...) { return false; }
}