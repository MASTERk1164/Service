@echo off
setlocal
cd /d "%~dp0"
call "%~dp0Build.cmd"
if errorlevel 1 (
  pause
  exit /b 1
)
"%~dp0ScheduledText.exe" /install
if errorlevel 1 pause
