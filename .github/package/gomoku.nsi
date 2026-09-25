; NSIS 安装包定义。
;
;   makensis -DSTAGE=<待打包目录> -DVERSION=1.2.3 -DARCH=x64 -DOUTFILE=<输出路径> gomoku.nsi
;
; 装到 %LOCALAPPDATA%\Gomoku，RequestExecutionLevel user，全程不弹 UAC。

Unicode true
SetCompressor /SOLID lzma

!ifndef STAGE
  !error "没给 -DSTAGE=<待打包目录>"
!endif
!ifndef VERSION
  !define VERSION "0.0.0"
!endif
!ifndef ARCH
  !define ARCH "x64"
!endif
!ifndef OUTFILE
  !define OUTFILE "Gomoku-${VERSION}-windows-${ARCH}-setup.exe"
!endif

!include "MUI2.nsh"

Name "五子棋"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Gomoku"
InstallDirRegKey HKCU "Software\Gomoku" "InstallDir"
RequestExecutionLevel user
ShowInstDetails show

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\Gomoku.exe"
!define MUI_FINISHPAGE_RUN_TEXT "立即启动五子棋"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"

Section "主程序" SecMain
  SetOutPath "$INSTDIR"
  ; /r 递归；用 \*.* 是为了把内容铺在 $INSTDIR 下而不是再套一层目录
  File /r "${STAGE}\*.*"

  WriteRegStr HKCU "Software\Gomoku" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\五子棋"
  CreateShortcut "$SMPROGRAMS\五子棋\五子棋.lnk" "$INSTDIR\Gomoku.exe"
  CreateShortcut "$SMPROGRAMS\五子棋\卸载.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$DESKTOP\五子棋.lnk" "$INSTDIR\Gomoku.exe"
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\五子棋.lnk"
  Delete "$SMPROGRAMS\五子棋\五子棋.lnk"
  Delete "$SMPROGRAMS\五子棋\卸载.lnk"
  RMDir "$SMPROGRAMS\五子棋"
  DeleteRegKey HKCU "Software\Gomoku"
  RMDir /r "$INSTDIR"
SectionEnd
