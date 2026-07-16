@echo off
setlocal enabledelayedexpansion

echo ================================================================
echo Building Coloruino (Release x64)...
echo ================================================================

:: Find Visual Studio 2022
set "VSWHERE="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath 2^>nul`) do set "VSWHERE=%%i"
if "%VSWHERE%"=="" (
    echo ERROR: Visual Studio 2022 not found.
    echo Please install Visual Studio 2022 with "Desktop development with C++".
    pause
    exit /b 1
)

call "%VSWHERE%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64

:: Build
msbuild coloruino5500.sln /p:Configuration=Release /p:Platform=x64 /m
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo ================================================================
echo Build succeeded.
echo Output: coloruino5500\x64\Release\pipanel.exe
echo ================================================================
pause