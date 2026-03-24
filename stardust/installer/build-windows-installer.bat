@echo off
REM =============================================================================
REM StarDust — Windows Installer Builder
REM Builds the plugin and creates an installer using Inno Setup
REM =============================================================================
REM
REM Prerequisites:
REM   - Visual Studio 2022 with C++ workload
REM   - CMake 3.22+ (in PATH)
REM   - Inno Setup 6+ (https://jrsoftware.org/isinfo.php)
REM
REM Usage:
REM   cd stardust
REM   installer\build-windows-installer.bat
REM =============================================================================

setlocal enabledelayedexpansion

echo === StarDust Windows Installer Builder ===
echo.

REM ---- Find project root ----------------------------------------------------
set "SCRIPT_DIR=%~dp0"
set "PROJECT_DIR=%SCRIPT_DIR%.."
pushd "%PROJECT_DIR%"
set "PROJECT_DIR=%CD%"
popd

set "BUILD_DIR=%PROJECT_DIR%\build"
set "ARTEFACTS=%BUILD_DIR%\StarDust_artefacts\Release"

REM ---- Step 1: Build --------------------------------------------------------
echo [1/3] Building StarDust...

if not exist "%ARTEFACTS%\VST3\StarDust.vst3" (
    cmake -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 "%PROJECT_DIR%"
    if errorlevel 1 (
        echo ERROR: CMake configuration failed.
        exit /b 1
    )
    cmake --build "%BUILD_DIR%" --config Release
    if errorlevel 1 (
        echo ERROR: Build failed.
        exit /b 1
    )
) else (
    echo   Build artefacts found, skipping build.
)

REM ---- Step 2: Verify artefacts ---------------------------------------------
echo [2/3] Verifying build artefacts...

if not exist "%ARTEFACTS%\VST3\StarDust.vst3" (
    echo ERROR: VST3 bundle not found.
    exit /b 1
)
echo   VST3: OK

if not exist "%ARTEFACTS%\Standalone\StarDust.exe" (
    echo WARNING: Standalone exe not found. Installer will include VST3 only.
)

REM ---- Step 3: Run Inno Setup ------------------------------------------------
echo [3/3] Building installer with Inno Setup...

set "ISCC="

REM Try common Inno Setup locations
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
)
if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"
)

REM Try PATH
if "!ISCC!"=="" (
    where ISCC.exe >nul 2>&1
    if not errorlevel 1 (
        for /f "delims=" %%i in ('where ISCC.exe') do set "ISCC=%%i"
    )
)

if "!ISCC!"=="" (
    echo ERROR: Inno Setup not found.
    echo   Install from: https://jrsoftware.org/isinfo.php
    echo   Or add ISCC.exe to your PATH.
    exit /b 1
)

echo   Using: !ISCC!

"!ISCC!" "%SCRIPT_DIR%StarDust-installer.iss"
if errorlevel 1 (
    echo ERROR: Inno Setup compilation failed.
    exit /b 1
)

echo.
echo === Done! ===
echo   Installer: %BUILD_DIR%\StarDust-Setup-1.0.0.exe
echo.
echo   To install silently: StarDust-Setup-1.0.0.exe /SILENT
echo.

endlocal
