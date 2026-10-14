@echo off
rem ELMonkey-Installer fuer Windows: holt Administratorrechte und startet das PowerShell-Skript.
rem Liegt das .ps1 nicht daneben, wird es aus dem Release geladen.
setlocal
set "PS1=%~dp0ELMonkey-Installer-Windows.ps1"
if not exist "%PS1%" (
  set "PS1=%TEMP%\ELMonkey-Installer-Windows.ps1"
  powershell -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol='Tls12'; Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/mozzi86/-archicad/releases/download/elm-sab-tapir-latest/ELMonkey-Installer-Windows.ps1' -OutFile '%TEMP%\ELMonkey-Installer-Windows.ps1'"
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File \"%PS1%\"'"
endlocal
