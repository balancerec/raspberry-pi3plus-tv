@echo off
setlocal
if not exist "%~dp0scripts\install-desktop-windows.cmd" (
  echo Extraia o pacote completo antes de abrir a Raspberry TV.
  pause
  exit /b 1
)
call "%~dp0scripts\install-desktop-windows.cmd"
exit /b %errorlevel%
