@echo off
setlocal
cd /d "%~dp0"
set "TEXT_CSC=%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%TEXT_CSC%" set "TEXT_CSC=%SystemRoot%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
if not exist "%TEXT_CSC%" (
  echo Windows .NET Framework C# compiler was not found.
  echo No files were downloaded. Build stopped.
  exit /b 1
)
echo Building App.cs, Schedule.cs and Installation.cs using Windows .NET Framework...
"%TEXT_CSC%" /nologo /warnaserror+ /target:winexe /platform:x64 /codepage:65001 /win32manifest:app.manifest /out:ScheduledText.exe /reference:System.dll /reference:System.Drawing.dll /reference:System.Windows.Forms.dll App.cs Schedule.cs Installation.cs Diagnostics.cs
if errorlevel 1 (
  echo Build failed. Do not run an older executable.
  exit /b 1
)
echo Built ScheduledText.exe in this folder.
exit /b 0
