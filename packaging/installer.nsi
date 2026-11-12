; MasterLens Windows installer (NSIS)
; 由 CI 调用：
;   makensis /DVERSION=3.1.0 /DARCH=x64 /DSTAGE=<abs stage> /DOUTFILE=<abs out> packaging/installer.nsi
Unicode true
!include "MUI2.nsh"

!ifndef VERSION
  !define VERSION "3.1.0"
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

; The release package must contain the FFmpeg runtime (guide §三 / §六).
!if /FILEEXISTS "${STAGE}\ffmpeg\ffmpeg.exe"
  !define HAVE_FFMPEG
!else
  !ifndef ALLOW_MISSING_RUNTIME
    !error "FFmpeg runtime missing in ${STAGE}\ffmpeg. Run scripts/fetch-ffmpeg.sh first, or pass /DALLOW_MISSING_RUNTIME=1 for a validation-only build."
  !endif
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
  ; NSIS exposes $COMMONFILES64 for C:\Program Files\Common Files.
  ; $COMMONPROGRAMFILES64 does not exist and only produces warning 6000,
  ; which sends the VST3 to the wrong directory on Windows x64.
  SetOutPath "$COMMONFILES64\VST3\MasterLens.vst3"
  File /r "${STAGE}\MasterLens.vst3\*.*"

  SetOutPath "$INSTDIR"
  File "${STAGE}\MasterLens.exe"
  File "${STAGE}\MasterLensCli.exe"

!ifdef HAVE_FFMPEG
  SetOutPath "$INSTDIR\ffmpeg"
  File "${STAGE}\ffmpeg\ffmpeg.exe"
!endif

  SetOutPath "$INSTDIR\Licenses"
  File /r "${STAGE}\Licenses\*.*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
  RMDir /r "$COMMONFILES64\VST3\MasterLens.vst3"
  Delete "$INSTDIR\MasterLens.exe"
  Delete "$INSTDIR\MasterLensCli.exe"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR\Licenses"
  RMDir /r "$INSTDIR\ffmpeg"
  RMDir "$INSTDIR"
SectionEnd
