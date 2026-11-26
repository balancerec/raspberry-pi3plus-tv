@echo off
setlocal
title Raspberry TV - Instalar icone
set "RTV_LAUNCHER_SOURCE=%~dp0"
powershell.exe -NoLogo -NoProfile -Command "$ErrorActionPreference='Stop'; $source=$env:RTV_LAUNCHER_SOURCE; $destination=Join-Path $env:LOCALAPPDATA 'Programs\RaspberryTV'; $marker=Join-Path $destination 'managed.txt'; if ((Test-Path -LiteralPath $destination) -and ((Get-ChildItem -LiteralPath $destination -Force | Measure-Object).Count -gt 0)) { if (!(Test-Path -LiteralPath $marker) -or ((Get-Content -LiteralPath $marker -Raw).Trim() -ne 'Raspberry TV Windows launcher v1')) { throw 'Pasta existente preservada; nao pertence a este instalador.' } }; $python='C:\Python314\pythonw.exe'; if (!(Test-Path -LiteralPath $python)) { throw 'Python Windows nao encontrado em C:\Python314.' }; New-Item -ItemType Directory -Path $destination -Force | Out-Null; foreach ($name in @('desktop-launcher.py','desktop-windows.py')) { Copy-Item -LiteralPath (Join-Path $source $name) -Destination (Join-Path $destination $name) -Force }; $iconSource=Join-Path $source 'raspberry-tv.ico'; if (!(Test-Path -LiteralPath $iconSource)) { $iconSource=Join-Path $source '..\native\raspberry-tv.ico' }; Copy-Item -LiteralPath $iconSource -Destination (Join-Path $destination 'raspberry-tv.ico') -Force; Set-Content -LiteralPath $marker -Value 'Raspberry TV Windows launcher v1' -Encoding ASCII; $desktop=[Environment]::GetFolderPath('Desktop'); $link=Join-Path $desktop 'Raspberry TV.lnk'; $shell=New-Object -ComObject WScript.Shell; if (Test-Path -LiteralPath $link) { $existing=$shell.CreateShortcut($link); if ($existing.Description -ne 'Raspberry TV native launcher (raspberry-tv)') { throw 'Atalho existente de outra origem preservado.' } }; $shortcut=$shell.CreateShortcut($link); $shortcut.TargetPath=$python; $shortcut.Arguments=[char]34+(Join-Path $destination 'desktop-windows.py')+[char]34; $shortcut.WorkingDirectory=$destination; $shortcut.IconLocation=(Join-Path $destination 'raspberry-tv.ico')+',0'; $shortcut.Description='Raspberry TV native launcher (raspberry-tv)'; $shortcut.WindowStyle=1; $shortcut.Save(); if (!(Test-Path -LiteralPath $link)) { throw 'O Windows nao salvou o atalho.' }; Write-Host ('Atalho criado: '+$link); Start-Process -FilePath $link -WindowStyle Hidden"
if errorlevel 1 (
  echo.
  echo Nao foi possivel instalar o icone. Veja a mensagem acima.
  pause
  exit /b 1
)
exit /b 0
