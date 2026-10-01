@echo off
rem Builds Leitmotif.exe on Windows.
rem Works from a "Developer Command Prompt for VS" (MSVC) or with MinGW-w64 gcc on PATH.
setlocal
cd /d "%~dp0"
set SRC=src\app.c src\content.c src\draw.c src\font.c src\mem.c src\stage.c src\synth.c src\tex.c src\platform_win32.c

where cl >nul 2>nul
if %errorlevel%==0 (
    echo Building with MSVC...
    cl /nologo /O2 /W3 /wd4244 /wd4305 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fe:Leitmotif.exe %SRC% /link /SUBSYSTEM:WINDOWS opengl32.lib gdi32.lib user32.lib winmm.lib
    if errorlevel 1 goto failed
    del /q *.obj >nul 2>nul
    goto built
)

where gcc >nul 2>nul
if %errorlevel%==0 (
    echo Building with MinGW gcc...
    gcc -O2 -std=c99 -mwindows -static -o Leitmotif.exe %SRC% -lopengl32 -lgdi32 -luser32 -lwinmm -lm
    if errorlevel 1 goto failed
    goto built
)

echo.
echo No C compiler found. Install Visual Studio Build Tools ("Desktop development with C++")
echo or MinGW-w64 (e.g. MSYS2: pacman -S mingw-w64-ucrt-x86_64-gcc), then run build.bat again.
pause
exit /b 1

:failed
echo Build failed.
pause
exit /b 1

:built
echo Built Leitmotif.exe
endlocal
