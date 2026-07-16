#include "HardwareInjector.h"
#include <iostream>
#include <vector>
#include <memory>

HardwareInjector::HardwareInjector()
    : m_handle(INVALID_HANDLE_VALUE)
{
}

HardwareInjector::~HardwareInjector()
{
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
}

bool HardwareInjector::Connect()
{
    if (m_handle != INVALID_HANDLE_VALUE)
        return true;

    std::cout << "[HardwareInjector] Searching for device..." << std::endl;

    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfo = SetupDiGetClassDevs(&hidGuid, nullptr, nullptr,
                                            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE) {
        std::cout << "[HardwareInjector] SetupDiGetClassDevs failed." << std::endl;
        return false;
    }

    SP_DEVICE_INTERFACE_DATA ifData = { sizeof(SP_DEVICE_INTERFACE_DATA) };
    DWORD idx = 0;
    bool found = false;

    while (SetupDiEnumDeviceInterfaces(devInfo, nullptr, &hidGuid, idx, &ifData)) {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetail(devInfo, &ifData, nullptr, 0, &needed, nullptr);
        if (needed == 0) { idx++; continue; }

        std::vector<BYTE> buf(needed);
        PSP_DEVICE_INTERFACE_DETAIL_DATA det = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(buf.data());
        det->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        if (!SetupDiGetDeviceInterfaceDetail(devInfo, &ifData, det, needed, nullptr, nullptr)) {
            idx++; continue;
        }

        HANDLE h = CreateFileW(det->DevicePath,
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            idx++; continue;
        }

        HIDD_ATTRIBUTES attrs = { sizeof(HIDD_ATTRIBUTES) };
        if (!HidD_GetAttributes(h, &attrs)) {
            CloseHandle(h);
            idx++; continue;
        }

        std::cout << "[HardwareInjector] Found device VID=0x" << std::hex << attrs.VendorID
                  << " PID=0x" << attrs.ProductID << std::dec << std::endl;

        if (attrs.VendorID != TARGET_VID || attrs.ProductID != TARGET_PID) {
            CloseHandle(h);
            idx++; continue;
        }

        std::cout << "[HardwareInjector] Device VID/PID match!" << std::endl;

        PHIDP_PREPARSED_DATA ppd = nullptr;
        if (!HidD_GetPreparsedData(h, &ppd)) {
            std::cout << "[HardwareInjector] HidD_GetPreparsedData failed." << std::endl;
            CloseHandle(h);
            idx++; continue;
        }

        HIDP_CAPS caps;
        NTSTATUS status = HidP_GetCaps(ppd, &caps);
        HidD_FreePreparsedData(ppd);

        if (status != HIDP_STATUS_SUCCESS) {
            std::cout << "[HardwareInjector] HidP_GetCaps failed." << std::endl;
            CloseHandle(h);
            idx++; continue;
        }

        std::cout << "[HardwareInjector] UsagePage=0x" << std::hex << caps.UsagePage
                  << " OutputReportByteLength=" << std::dec << caps.OutputReportByteLength << std::endl;

        // We need the vendor interface (UsagePage 0xFF00) with output report length >= 7
        if (caps.UsagePage != 0xFF00 || caps.OutputReportByteLength < INJECT_REPORT_SIZE) {
            std::cout << "[HardwareInjector] Interface does not match vendor criteria." << std::endl;
            CloseHandle(h);
            idx++; continue;
        }

        // Test with a SET_REPORT probe
        BYTE probe[INJECT_REPORT_SIZE] = { REPORT_ID, 0x00 };
        if (HidD_SetFeature(h, probe, caps.OutputReportByteLength)) {
            std::cout << "[HardwareInjector] Probe successful. Device connected." << std::endl;
            m_handle = h;
            found = true;
            break;
        } else {
            std::cout << "[HardwareInjector] HidD_SetFeature probe failed." << std::endl;
            CloseHandle(h);
            idx++;
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return found;
}

bool HardwareInjector::SendReport(const BYTE* report, size_t len)
{
    if (m_handle == INVALID_HANDLE_VALUE || len > REPORT_SIZE)
        return false;

    return HidD_SetFeature(m_handle, (PVOID)report, (ULONG)REPORT_SIZE) != FALSE;
}

void HardwareInjector::SendCommand(BYTE cmd, const BYTE* data, size_t dataLen)
{
    if (m_handle == INVALID_HANDLE_VALUE || dataLen > 5)
        return;

    BYTE report[REPORT_SIZE] = { REPORT_ID, cmd };
    memcpy(report + 2, data, dataLen);
    for (size_t i = 2 + dataLen; i < REPORT_SIZE; i++)
        report[i] = 0;

    SendReport(report, REPORT_SIZE);
}

void HardwareInjector::SendMovement(int16_t dx, int16_t dy)
{
    BYTE data[4];
    data[0] = (BYTE)(dx & 0xFF);
    data[1] = (BYTE)((dx >> 8) & 0xFF);
    data[2] = (BYTE)(dy & 0xFF);
    data[3] = (BYTE)((dy >> 8) & 0xFF);
    SendCommand(0x01, data, 4);
}

void HardwareInjector::SendCooldown(uint16_t ms)
{
    BYTE data[2];
    data[0] = (BYTE)(ms & 0xFF);
    data[1] = (BYTE)((ms >> 8) & 0xFF);
    SendCommand(0x02, data, 2);
}

void HardwareInjector::Move(int dx, int dy)
{
    SendMovement((int16_t)dx, (int16_t)dy);
}

void HardwareInjector::Click()
{
    // Not used; hardware injector handles click via SilentAim/Flick
}

void HardwareInjector::SilentAim(int dx, int dy)
{
    SendMovement((int16_t)dx, (int16_t)dy);
}

void HardwareInjector::Flick(int dx, int dy)
{
    SendMovement((int16_t)dx, (int16_t)dy);
}

void HardwareInjector::SetCooldown(int ms)
{
    SendCooldown((uint16_t)ms);
}