@echo off
rem Double-click to render the whole Night Wind walkthrough to night-wind.mp4 (in this folder).
cd /d "%~dp0"
where python >nul 2>nul || (echo Python is not installed. Get it from https://www.python.org/downloads/ and tick "Add python.exe to PATH". & pause & exit /b 1)
python -m pip install --quiet --upgrade pillow numpy imageio-ffmpeg
python make_video.py %*
pause
