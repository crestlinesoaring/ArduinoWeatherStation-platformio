@echo off
REM Runs telnet-snipe.ps1 without changing PowerShell execution policy.
setlocal
set "SCRIPT=%~dp0telnet-snipe.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %*
