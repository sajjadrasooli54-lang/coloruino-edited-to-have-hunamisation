@echo off
setlocal enabledelayedexpansion

:: ------------------------------------------------------------
:: Coloruino Launcher – Safe & Subtle Process Name Spoofing
:: Uses a fixed legitimate-sounding name (DispCalHelper.exe)
:: and places it in a system-like folder to blend in.
:: No process killing – just launches.
:: ------------------------------------------------------------

:: ── Fixed spoofed name ──────────────────────────────────────────
set "SPOOF_NAME=DispCalHelper.exe"

:: ── Use a system-like folder (not Temp) ────────────────────────
set "SPOOF_DIR=%APPDATA%\Microsoft\Windows\Caches"
if not exist "%SPOOF_DIR%" mkdir "%SPOOF_DIR%"

:: ── Path to the real executable ─────────────────────────────────
set "SOURCE_EXE=%~dp0coloruino-app\x64\Release\pipanel.exe"
set "TARGET_EXE=%SPOOF_DIR%\%SPOOF_NAME%"

:: ── Copy only if source is newer or target missing ─────────────
if not exist "%TARGET_EXE%" (
    echo [*] First run – copying executable to %SPOOF_DIR%
    copy /y "%SOURCE_EXE%" "%TARGET_EXE%" >nul
) else (
    :: Simple check: compare file dates
    for %%A in ("%SOURCE_EXE%") do set "src_date=%%~tA"
    for %%B in ("%TARGET_EXE%") do set "tgt_date=%%~tB"
    if "!src_date!" gtr "!tgt_date!" (
        echo [*] Updating executable...
        copy /y "%SOURCE_EXE%" "%TARGET_EXE%" >nul
    )
)

:: ── Launch the spoofed process ──────────────────────────────────
start "" "%TARGET_EXE%"

echo.
echo [✓] Application launched as %SPOOF_NAME%.
echo [✓] WebUI available at http://localhost:13548/
echo.
echo Press any key to close this window...
pause >nul