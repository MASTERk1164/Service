Unicode true
!include "MUI2.nsh"
Name "예약 클릭"
OutFile "../dist/Setup.exe"
InstallDir "$LOCALAPPDATA\ScheduledClick"
InstallDirRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "InstallLocation"
RequestExecutionLevel user
VIProductVersion "1.6.2.0"
VIAddVersionKey /LANG=1042 "ProductName" "예약 클릭"
VIAddVersionKey /LANG=1042 "FileDescription" "예약 클릭 설치 프로그램"
VIAddVersionKey /LANG=1042 "FileVersion" "1.6.2"
VIAddVersionKey /LANG=1042 "LegalCopyright" ""
!define MUI_ICON "../assets/app.ico"
!define MUI_UNICON "../assets/app.ico"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Korean"
Section "예약 클릭"
SetShellVarContext current
SetOutPath "$INSTDIR"
File "../dist/ScheduledClick.exe"
File "../README.md"
File "../SECURITY-REVIEW.md"
CreateDirectory "$SMPROGRAMS\예약 클릭"
CreateShortcut "$SMPROGRAMS\예약 클릭\예약 클릭.lnk" "$INSTDIR\ScheduledClick.exe"
CreateShortcut "$DESKTOP\예약 클릭.lnk" "$INSTDIR\ScheduledClick.exe"
WriteUninstaller "$INSTDIR\Uninstall.exe"
WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "DisplayName" "예약 클릭"
WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "UninstallString" '"$INSTDIR\Uninstall.exe"'
WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "DisplayVersion" "1.6.2"
WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "DisplayIcon" '"$INSTDIR\ScheduledClick.exe",0'
WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "InstallLocation" "$INSTDIR"
WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "NoModify" 1
WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick" "NoRepair" 1
SectionEnd
Section "Uninstall"
SetShellVarContext current
Delete "$INSTDIR\ScheduledClick.exe"
Delete "$INSTDIR\README.md"
Delete "$INSTDIR\SECURITY-REVIEW.md"
Delete "$INSTDIR\Uninstall.exe"
Delete "$DESKTOP\예약 클릭.lnk"
Delete "$SMPROGRAMS\예약 클릭\예약 클릭.lnk"
RMDir "$SMPROGRAMS\예약 클릭"
RMDir "$INSTDIR"
DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ScheduledClick"
SectionEnd
