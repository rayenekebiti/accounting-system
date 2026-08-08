; Occountant — Windows installer (Inno Setup 6.x).
;
; Built by tools/release.sh, which stages a CLEAN runtime tree (tools/stage-runtime.sh) and then
; invokes:  iscc /DAppVersion=<v> /DVersionCode=<code> /DStageDir=<abs> /DOutputDir=<abs> installer\Occountant.iss
;
; ── Load-bearing design decisions ────────────────────────────────────────────
;   • {app} (Program Files) holds ONLY binaries — fully replaceable on upgrade.
;   • USER DATA lives in %LOCALAPPDATA%\Occountant (QStandardPaths AppDataLocation) which this
;     installer NEVER reads, writes, or deletes. Install / upgrade / uninstall therefore cannot
;     lose books, journals, or settings.
;   • DOWNGRADE REFUSAL (InitializeSetup) mirrors appinfo::isDowngrade(): an older setup will not
;     install over a newer one, so an old build can never reopen books a newer build wrote.
;   • Same AppId across versions → Inno upgrades in place; AppMutex blocks installing while running.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef VersionCode
  #define VersionCode "1000000"
#endif
#ifndef StageDir
  #define StageDir "..\dist\Occountant"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif
; Optional: pass /DSignToolCmd="signtool ..." (or configure a SignTool in the IDE) to sign.

#define AppName      "Occountant"
#define AppPublisher "RIO&JHK Technologies Co."
#define AppExeName   "AccountingQuick.exe"
#define AppURL       "https://github.com/rayenekebiti/accounting-system"

[Setup]
; NEVER change AppId — it is how a new installer recognises + upgrades a prior install.
AppId={{B6F4C9A2-8E71-4C33-9D2A-0F1E7A6B5C40}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayName={#AppName} {#AppVersion}
UninstallDisplayIcon={app}\{#AppExeName}
OutputDir={#OutputDir}
OutputBaseFilename={#AppName}-{#AppVersion}-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
; Occountant is 64-bit only.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; Program Files + HKLM require elevation.
PrivilegesRequired=admin
; Refuse to install (or uninstall) while a copy is running — matches the C++ single-instance model.
AppMutex=Occountant.SingleInstance
AllowNoIcons=yes
; Installer-exe file metadata (Details tab / SmartScreen publisher line).
VersionInfoVersion={#AppVersion}
VersionInfoProductVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} Setup
VersionInfoCopyright=© {#AppPublisher}
#ifdef SignToolCmd
SignTool=byname
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "assococc";    Description: "Associate .occ backup files with {#AppName}"; GroupDescription: "File associations:"; Flags: unchecked

[Files]
; The entire clean, staged runtime tree (exe + Qt runtime + plugins + qml + DLL closure).
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}";                       Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";                 Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Registry]
; Version + install location the NEXT installer reads (upgrade detection + downgrade refusal).
Root: HKLM; Subkey: "Software\{#AppName}"; ValueType: string; ValueName: "InstallDir";   ValueData: "{app}";           Flags: uninsdeletevalue
Root: HKLM; Subkey: "Software\{#AppName}"; ValueType: string; ValueName: "Version";      ValueData: "{#AppVersion}";   Flags: uninsdeletevalue
Root: HKLM; Subkey: "Software\{#AppName}"; ValueType: string; ValueName: "VersionCode";  ValueData: "{#VersionCode}";  Flags: uninsdeletevalue uninsdeletekeyifempty
; Optional .occ association (backup archive) — only when the task is selected.
Root: HKLM; Subkey: "Software\Classes\.occ";                   ValueType: string; ValueName: ""; ValueData: "Occountant.Backup"; Flags: uninsdeletevalue; Tasks: assococc
Root: HKLM; Subkey: "Software\Classes\Occountant.Backup";      ValueType: string; ValueName: ""; ValueData: "Occountant Backup"; Flags: uninsdeletekey; Tasks: assococc
Root: HKLM; Subkey: "Software\Classes\Occountant.Backup\DefaultIcon";      ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExeName},0"; Tasks: assococc
Root: HKLM; Subkey: "Software\Classes\Occountant.Backup\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExeName}"" ""%1"""; Tasks: assococc

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

; NOTE: there is deliberately NO [UninstallDelete] for user data. Uninstall removes only {app},
; the shortcuts, and the Software\Occountant keys above. %LOCALAPPDATA%\Occountant is preserved —
; uninstalling must never destroy a business's accounting records.

[Code]
function ParseInt64(const S: String; Default: Int64): Int64;
var
  I: Integer;
  R: Int64;
begin
  R := 0;
  if S = '' then begin Result := Default; exit; end;
  for I := 1 to Length(S) do begin
    if (S[I] < '0') or (S[I] > '9') then begin Result := Default; exit; end;
    R := (R * 10) + (Ord(S[I]) - Ord('0'));
  end;
  Result := R;
end;

function InstalledVersionCode(): Int64;
var
  S: String;
begin
  Result := 0;
  if RegQueryStringValue(HKLM, 'Software\{#AppName}', 'VersionCode', S) then
    Result := ParseInt64(S, 0);
end;

// Mirrors appinfo::isDowngrade(): a setup strictly OLDER than what is installed is refused.
function InitializeSetup(): Boolean;
var
  Installed, Setup: Int64;
begin
  Result := True;
  Installed := InstalledVersionCode();
  Setup := ParseInt64('{#VersionCode}', 0);
  if (Installed > 0) and (Setup < Installed) then begin
    MsgBox('A newer version of {#AppName} is already installed on this computer.'
           + #13#10 + 'Downgrading is not supported, because an older build must never'
           + #13#10 + 'reopen accounting data written by a newer one.'
           + #13#10#13#10 + 'Setup will now exit. Your data has not been touched.',
           mbCriticalError, MB_OK);
    Result := False;
  end;
end;
