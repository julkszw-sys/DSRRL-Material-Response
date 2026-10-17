@echo off
setlocal
cd /d "%~dp0" || exit /b 1
rem PS1 auto-resolves its game root; no trailing-slash quote ambiguity.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0DSRRL_SAFE_CLEAN_LOWLOD.ps1" -Apply
set "result=%errorlevel%"
if not "%result%"=="0" echo [DSRRL] CZYSZCZENIE: BLAD %result% - sprawdz raport.
if not defined GITHUB_ACTIONS pause
exit /b %result%
