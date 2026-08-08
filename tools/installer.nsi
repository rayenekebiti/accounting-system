; installer.nsi — Occountant Windows installer (NSIS).
;
; DEPRECATED / LEGACY: the SHIPPING installer is installer/Occountant.iss (Inno Setup), built by
; tools/release.sh. This NSIS script is kept only as a reference / emergency fallback for
; environments that have makensis but not iscc. It is NOT wired into release.sh.
;
; Upgrade-safety model (the load-bearing design decision):
;   • PROGRAM FILES (this installer's $INSTDIR) holds ONLY binaries — replaceable.
;   • USER DATA lives in %LOCALAPPDATA%\Occountant (QStandardPaths AppDataLocation),
;     which the installer NEVER reads, writes, or deletes.
;   Therefore install / upgrade / uninstall cannot lose books, journals, or settings.
;
;   On upgrade we silently run the previous version's uninstaller first, so no stale
;   DLLs or plugins linger to conflict with the new Qt runtime — but because data is
;   elsewhere, that uninstall is data-safe.
;
; Build via tools/package-win.sh (stages a clean runtime tree, then invokes makensis).

Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

!define APPNAME   "Occountant"
!define COMPANY   "RIO&JHK Technologies Co."
!define EXENAME   "AccountingQuick.exe"
!ifndef VERSION
  !define VERSION "1.0.0"
!endif
!ifndef STAGE
  !define STAGE "..\dist\Occountant"
!endif
!ifndef OUTFILE
  !define OUTFILE "..\dist\Occountant-${VERSION}-Setup.exe"
!endif

!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

Name "${APPNAME} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey HKLM "Software\${APPNAME}" "InstallDir"
RequestExecutionLevel admin          ; Program Files + HKLM need elevation
SetCompressor /SOLID lzma

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

; ── Upgrade detection: remove the previous version BEFORE laying down the new one,
;    so no orphaned DLL/plugin from an older Qt can shadow the new runtime. ──────────
Function .onInit
  SetRegView 64
  ReadRegStr $R0 HKLM "${UNINST_KEY}" "UninstallString"
  ReadRegStr $R1 HKLM "Software\${APPNAME}" "InstallDir"
  ${If} $R0 != ""
  ${AndIf} $R1 != ""
    DetailPrint "Upgrading: removing the previous version (your data is preserved)…"
    ; _?=$R1 runs the uninstaller in place (synchronous) without copying it to temp,
    ; so ExecWait actually blocks until it finishes.
    ExecWait '"$R0" /S _?=$R1'
  ${EndIf}
FunctionEnd

Section "Install"
  SetRegView 64
  SetOutPath "$INSTDIR"

  ; Belt-and-braces: clear plugin/module trees that must not accumulate stale files
  ; across versions (data is in %LOCALAPPDATA%, never here, so this is binary-only).
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\imageformats"
  RMDir /r "$INSTDIR\styles"
  RMDir /r "$INSTDIR\tls"
  RMDir /r "$INSTDIR\generic"
  RMDir /r "$INSTDIR\networkinformation"
  RMDir /r "$INSTDIR\qml"

  File /r "${STAGE}\*"

  ; Version + install location for the next upgrade to find.
  WriteRegStr HKLM "Software\${APPNAME}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\${APPNAME}" "Version"    "${VERSION}"

  ; Add/Remove Programs entry.
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayName"     "${APPNAME}"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayVersion"  "${VERSION}"
  WriteRegStr   HKLM "${UNINST_KEY}" "Publisher"       "${COMPANY}"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayIcon"     "$INSTDIR\${EXENAME}"
  WriteRegStr   HKLM "${UNINST_KEY}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr   HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut  "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"  "$INSTDIR\${EXENAME}"
  CreateShortcut  "$SMPROGRAMS\${APPNAME}\Uninstall.lnk"   "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
  SetRegView 64
  ; Remove ONLY program files, shortcuts, and registry. User data in
  ; %LOCALAPPDATA%\${APPNAME} is intentionally left intact (uninstall must never
  ; destroy a business's accounting records).
  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\Uninstall.lnk"
  RMDir  "$SMPROGRAMS\${APPNAME}"
  DeleteRegKey HKLM "${UNINST_KEY}"
  DeleteRegKey HKLM "Software\${APPNAME}"
SectionEnd
