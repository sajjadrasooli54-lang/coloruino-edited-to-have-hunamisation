/*
 * HardwareInjector.h
 * Hardware-based mouse injection via HID Feature Reports (USB).
 * Communicates with the Arduino running tfirm firmware (PRO X 2 spoof).
 * Uses Interface 2, Report ID 0x10 to send movement and cooldown commands.
 */

#pragma once

#include "IMouseInjector.h"
#include <string>
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

class HardwareInjector : public IMouseInjector {
public:
    HardwareInjector();
    ~HardwareInjector() override;

    // Try to open the device. Returns true on success.
    bool Connect();

    // IMouseInjector implementation
    void Move(int dx, int dy) override;
    void Click() override;   // Not used in hardware mode (firmware handles click via P/F)
    void SilentAim(int dx, int dy) override;
    void Flick(int dx, int dy) override;
    void SetCooldown(int ms) override;

    // Check if device is currently connected
    bool IsConnected() const { return m_handle != INVALID_HANDLE_VALUE; }

private:
    HANDLE m_handle;
    static constexpr WORD TARGET_VID = 0x046D;
    static constexpr WORD TARGET_PID = 0xC09B;
    static constexpr BYTE REPORT_ID = 0x10;
    static constexpr size_t REPORT_SIZE = 7;  // 1 byte ID + 6 bytes payload

    // Send a raw HID feature report (SET_REPORT via IOCTL)
    bool SendReport(const BYTE* report, size_t len);

    // Internal command helpers
    void SendCommand(BYTE cmd, const BYTE* data, size_t dataLen);
    void SendMovement(int16_t dx, int16_t dy);
    void SendCooldown(uint16_t ms);
};