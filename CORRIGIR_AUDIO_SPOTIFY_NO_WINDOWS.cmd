@echo off
setlocal
if not exist "%~dp0scripts\release.json" (
  echo Extraia o pacote central completo antes de corrigir o audio.
  pause
  exit /b 1
)
set "RTV_AUDIO_FIX=1"
call "%~dp0scripts\install-desktop-windows.cmd"
exit /b %errorlevel%
