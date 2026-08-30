@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\windows\setup.ps1" %*
if errorlevel 1 (
  echo.
  echo Setup did not complete. Read the message above.
  pause
)
endlocal
