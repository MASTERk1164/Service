Unicode true
!include "MUI2.nsh"
Name "예약 클릭"
OutFile "../dist/Setup.exe"
InstallDir "$LOCALAPPDATA\ScheduledClick"
RequestExecutionLevel user
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Korean"
Section "예약 클릭"
SetOutPath "$INSTDIR"
File "../dist/ScheduledClick.exe"
File "../README.md"
CreateDirectory "$SMPROGRAMS\예약 클릭"
CreateShortcut "$SMPROGRAMS\예약 클릭\예약 클릭.lnk" "$INSTDIR\ScheduledClick.exe"
CreateShortcut "$DESKTOP\예약 클릭.lnk" "$INSTDIR\ScheduledClick.exe"
WriteUninstaller "$INSTDIR\Uninstall.exe"
WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "DisplayName" "예약 클릭"
WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "UninstallString" '"$INSTDIR\Uninstall.exe"'
WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "DisplayVersion" "1.0.0"
WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "NoModify" 1
WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "NoRepair" 1
SectionEnd
Section "Uninstall"
Delete "$INSTDIR\ScheduledClick.exe"
Delete "$INSTDIR\README.md"
Delete "$INSTDIR\Uninstall.exe"
Delete "$DESKTOP\예약 클릭.lnk"
Delete "$SMPROGRAMS\예약 클릭\예약 클릭.lnk"
RMDir "$SMPROGRAMS\예약 클릭"
RMDir "$INSTDIR"
DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick"
SectionEnd
