@echo off
setlocal
cd /d "%~dp0" || exit /b 1
rem Use the PS1's own folder for the game root. Never pass "%~dp0" as
rem an explicit game folder parameter: trailing backslashes cause quote parsing errors.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0DSRRL_SAFE_CLEAN_LOWLOD.ps1"
set "result=%errorlevel%"
if not "%result%"=="0" echo [DSRRL] AUDYT: BLAD %result% - niczego nie przeniesiono.
if not defined GITHUB_ACTIONS pause
exit /b %result%
