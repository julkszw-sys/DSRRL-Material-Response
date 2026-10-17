@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0DSRRL_SAFE_CLEAN_LOWLOD.ps1" -GameDir "%~dp0"
echo.
pause
