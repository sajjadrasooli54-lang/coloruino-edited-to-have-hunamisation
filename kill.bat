@echo off
echo ================================================================
echo Coloruino Cleanup
echo ================================================================

:: ── Kill running processes ─────────────────────────────────────────
taskkill /f /im pipanel.exe 2>nul
if errorlevel 1 ( echo [*] No pipanel.exe running. ) else ( echo [✓] Killed pipanel.exe )

:: ── Delete config files ────────────────────────────────────────────
set "DATA_DIR=%LOCALAPPDATA%\AMD\RadeonSettings"
if exist "%DATA_DIR%\data" (
    del /f /q "%DATA_DIR%\data" 2>nul
    echo [✓] Deleted %DATA_DIR%\data
) else (
    echo [*] data not found.
)

set "AUTH_FILE=%~dp0auth.dat"
if exist "%AUTH_FILE%" (
    del /f /q "%AUTH_FILE%" 2>nul
    echo [✓] Deleted %AUTH_FILE%
) else (
    echo [*] auth.dat not found.
)

:: ── Remove firewall rule (requires admin) ─────────────────────────
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Firewall rule deletion requires Administrator privileges.
    echo [!] Please run as Administrator or delete manually:
    echo     netsh advfirewall firewall delete rule name="AMD Radeon Software Helper"
) else (
    netsh advfirewall firewall delete rule name="AMD Radeon Software Helper" >nul 2>&1
    if %errorlevel% equ 0 (
        echo [✓] Removed firewall rule "AMD Radeon Software Helper"
    ) else (
        echo [*] Firewall rule not found or already removed.
    )
)

echo.
echo ================================================================
echo Cleanup complete. Press any key to exit.
pause >nul