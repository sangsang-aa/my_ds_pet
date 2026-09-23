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
rem  Path (b) - MinGW-w64:
rem    x86_64-w64-mingw32-g++ (or g++) on PATH. cmd.exe does NOT expand
rem    wildcards and g++ does not either, so the source list is generated
rem    with `dir /b /s` and passed via an @response file.
rem
rem  Path (c) - fallback: CMake with the Visual Studio generator.
rem ============================================================
setlocal
cd /d "%~dp0"

where cl >nul 2>nul
if %errorlevel%==0 (
    cl /nologo /EHsc /std:c++17 /O2 src\*.cpp src\core\*.cpp src\pet\*.cpp src\bubble\*.cpp /I src gdiplus.lib user32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS /OUT:pet_demo.exe
    if not errorlevel 1 (
        echo [pet] build OK: pet_demo.exe via cl
        exit /b 0
    )
    echo [pet] cl reported an error, trying g++ instead...
)

where g++ >nul 2>nul
if %errorlevel%==0 (
    if not exist build mkdir build
    dir /b /s src\*.cpp > build\sources.txt
    g++ -std=c++17 -O2 -mwindows -Isrc @build\sources.txt -lgdiplus -luser32 -lgdi32 -o pet_demo.exe
    if not errorlevel 1 (
        echo [pet] build OK: pet_demo.exe via g++
        del build\sources.txt
        exit /b 0
    )
    echo [pet] g++ reported an error, trying CMake instead...
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
echo [pet] ERROR: none of cl.exe, g++ and cmake.exe were found on PATH.
echo [pet] Either open an "x64 Native Tools Command Prompt for VS 2022",
echo [pet] or put MinGW-w64's g++ (or cmake) on PATH, then run build.bat again.
exit /b 1

:fail
echo [pet] build FAILED.
exit /b 1
