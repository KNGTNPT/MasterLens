; MasterLens Windows installer (NSIS)
; 由 CI 调用：
;   makensis /DVERSION=2.1.1 /DARCH=x64 /DSTAGE=<abs stage> /DOUTFILE=<abs out> packaging/installer.nsi
Unicode true
!include "MUI2.nsh"

!ifndef VERSION
  !define VERSION "2.1.1"
!endif
!ifndef ARCH
  !define ARCH "x64"
!endif
!ifndef STAGE
  !define STAGE "stage-x64"
!endif
!ifndef OUTFILE
  !define OUTFILE "MasterLens-${VERSION}-Windows-${ARCH}.exe"
!endif

Name "MasterLens ${VERSION} — BerryStudio (${ARCH})"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\MasterLens"
RequestExecutionLevel admin

!define MUI_WELCOMEPAGE_TITLE "MasterLens ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "母带响度 · 平台归一化 · 信号健康$\r$\nby BerryStudio · Developer KNGTNPT 兰$\r$\n$\r$\nThis installer places the VST3 plug-in and the standalone analyser."

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "MasterLens (VST3 + Standalone + CLI)"
  SetOutPath "$COMMONPROGRAMFILES64\VST3\MasterLens.vst3"
  File /r "${STAGE}\MasterLens.vst3\*.*"

  SetOutPath "$INSTDIR"
  File "${STAGE}\MasterLens.exe"
  File "${STAGE}\MasterLensCli.exe"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
  RMDir /r "$COMMONPROGRAMFILES64\VST3\MasterLens.vst3"
  Delete "$INSTDIR\MasterLens.exe"
  Delete "$INSTDIR\MasterLensCli.exe"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd
