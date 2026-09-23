@echo off
rem ============================================================
rem  Desktop Pet Demo - build script.
rem
rem  Path (a) - PRIMARY, simplest: MSVC one-liner via cl.
rem    NOTE: run this script from an "x64 Native Tools Command
rem    Prompt for VS 2022" (Start menu -> Visual Studio 2022 ->
rem    "x64 Native Tools Command Prompt for VS 2022"), or from any
rem    shell where cl.exe is on PATH.
rem
rem  Path (b) - fallback: CMake with the Visual Studio generator.
rem ============================================================
setlocal
cd /d "%~dp0"

where cl >nul 2>nul
if %errorlevel%==0 (
    cl /nologo /EHsc /std:c++17 /O2 src\*.cpp src\core\*.cpp src\pet\*.cpp src\bubble\*.cpp /I src gdiplus.lib user32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS /OUT:pet_demo.exe
    if not errorlevel 1 (
        echo [pet] build OK: pet_demo.exe (cl)
        exit /b 0
    )
    echo [pet] cl reported an error, trying CMake instead...
)

where cmake >nul 2>nul
if errorlevel 1 goto :nobuild

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fail
cmake --build build --config Release
if errorlevel 1 goto :fail
echo [pet] build OK: build\Release\pet_demo.exe
exit /b 0

:nobuild
echo [pet] ERROR: cl.exe and cmake.exe were both not found.
echo [pet] Open an "x64 Native Tools Command Prompt for VS 2022" and run build.bat again.
exit /b 1

:fail
echo [pet] build FAILED.
exit /b 1
