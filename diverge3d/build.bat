@echo off
rem Builds DivergeE5Workshop.exe on Windows.
rem Works from a "Developer Command Prompt for VS" (MSVC) or with MinGW-w64 gcc on PATH.
setlocal
cd /d "%~dp0"
set SRC=src\app.c src\bike.c src\content.c src\diagrams.c src\font.c src\mem.c src\mesh.c src\sim.c src\ui.c src\platform_win32.c

where cl >nul 2>nul
if %errorlevel%==0 (
    echo Building with MSVC...
    cl /nologo /O2 /W3 /wd4244 /wd4305 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fe:DivergeE5Workshop.exe %SRC% /link /SUBSYSTEM:WINDOWS opengl32.lib gdi32.lib user32.lib
    if errorlevel 1 goto failed
    del /q *.obj >nul 2>nul
    goto built
)

where gcc >nul 2>nul
if %errorlevel%==0 (
    echo Building with MinGW gcc...
    gcc -O2 -std=c99 -mwindows -static -o DivergeE5Workshop.exe %SRC% -lopengl32 -lgdi32 -luser32 -lm
    if errorlevel 1 goto failed
    goto built
)

echo.
echo No C compiler found.
echo Install one of these, then run build.bat again:
echo   - Visual Studio Build Tools (free): choose "Desktop development with C++",
echo     then open "Developer Command Prompt for VS" and run build.bat from there.
echo   - MinGW-w64 (for example via MSYS2: pacman -S mingw-w64-ucrt-x86_64-gcc),
echo     with its bin folder on PATH.
pause
exit /b 1

:failed
echo Build failed.
pause
exit /b 1

:built
echo Built DivergeE5Workshop.exe
endlocal
