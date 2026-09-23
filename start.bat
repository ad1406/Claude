@echo off
rem Double-click to run Giant-Scale On-Call locally on Windows.
cd /d "%~dp0"
where node >nul 2>nul
if errorlevel 1 (
  echo Node.js is not installed. Install the LTS version from https://nodejs.org
  echo or run:  winget install OpenJS.NodeJS.LTS   then open a new terminal.
  pause
  exit /b 1
)
if not exist .env (
  copy .env.example .env >nul
  echo Created .env. Paste your DeepSeek key after DEEPSEEK_API_KEY= then save and close Notepad.
  notepad .env
)
if not defined HOST set HOST=127.0.0.1
if not defined PORT set PORT=8080
start "" cmd /c "timeout /t 2 >nul & start http://localhost:%PORT%"
node server.js
pause
