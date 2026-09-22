@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"

set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Release"
if /I "%CONFIG%"=="release" set "CONFIG=Release"
if /I "%CONFIG%"=="debug" set "CONFIG=Debug"
if not "%CONFIG%"=="Release" if not "%CONFIG%"=="Debug" (
    echo ERROR: Build configuration must be Release or Debug.
    echo Usage: build-vs2022.bat [Release^|Debug]
    exit /b 1
)

echo FlatOut 4 VR SimHub Extractor
echo Visual Studio 2022 x64 %CONFIG% build

echo.
set "ROOT=%CD%"
set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat`) do if not defined VCVARS set "VCVARS=%%I"
)
if not defined VCVARS (
    echo ERROR: Visual Studio C++ x64 tools were not found.
    exit /b 1
)

echo Config:  %CONFIG%
echo.
call "%VCVARS%" >nul
if "%CONFIG%"=="Release" (set "FLAGS=/nologo /std:c++17 /W4 /permissive- /EHsc /utf-8 /O2 /MT") else (set "FLAGS=/nologo /std:c++17 /W4 /permissive- /EHsc /utf-8 /Od /Zi /MTd")
if not exist out\%CONFIG% mkdir out\%CONFIG%
if not exist out\%CONFIG%\obj mkdir out\%CONFIG%\obj
cl %FLAGS% /Foout\%CONFIG%\obj\ /Fdout\%CONFIG%\extractor.pdb /Fe:out\%CONFIG%\extractor.exe src\main.cpp src\config.cpp src\telemetry.cpp src\simhub.cpp /link /SUBSYSTEM:WINDOWS /PDB:out\%CONFIG%\extractor.pdb Ws2_32.lib Winmm.lib
if errorlevel 1 (
    echo.
    echo BUILD FAILED.
    exit /b 1
)

echo.
echo BUILD PASSED.
echo Extractor:
echo   %ROOT%\out\%CONFIG%\extractor.exe
echo.
echo Next: run package-release.bat
exit /b 0
