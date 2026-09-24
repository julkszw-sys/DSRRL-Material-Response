@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build_windows_ptde_material_response_v2_11.ps1" %*
exit /b %ERRORLEVEL%
