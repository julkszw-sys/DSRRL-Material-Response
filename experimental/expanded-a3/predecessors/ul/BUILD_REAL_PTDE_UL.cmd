@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build_windows_real_ptde_ul.ps1" %*
if errorlevel 1 (
  echo.
  echo BUILD FAILED
  pause
  exit /b 1
)
echo.
echo BUILD PASS: build-msvc\bin\DSRRL_REAL_PTDE_UL_LIVE.addon64
pause
