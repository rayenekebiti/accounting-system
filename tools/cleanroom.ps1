# cleanroom.ps1 — automated clean-environment smoke test.
#
# Proves the deployed build runs on a machine WITHOUT the dev toolchain: PATH is
# stripped to bare Windows (no MSYS2, no ucrt64, no Qt). The app must resolve every
# DLL/plugin/QML-module/translation from its own deploy tree. Uses the ACCT_PROBE
# harness, which loads the full engine, switches to Arabic (RTL + translator),
# opens the customer editor, and reads persistence — then self-quits.
#
# Asserts: clean exit, and that startup / translation-load / RTL / persistence /
# invoice+customer load / editor-open all happened. Hard-error dialogs are suppressed
# and the process is killed on timeout so a missing dependency can never hang CI.
#
# Usage:  powershell -ExecutionPolicy Bypass -File tools/cleanroom.ps1 [-Exe <path>]
# Exit 0 = deployable.
param(
  [string]$Exe = "$PSScriptRoot\..\build\AccountingQuick.exe"
)
$ErrorActionPreference = "Stop"
$Exe = (Resolve-Path $Exe).Path
$probe = Join-Path $env:TEMP "cleanroom_smoke.txt"
$data  = Join-Path $env:TEMP "cleanroom_smoke_data"
Remove-Item $probe -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $data | Out-Null

# Suppress hard-error dialogs so a missing DLL can't block the run.
$sig = '[DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint mode);'
$k = Add-Type -MemberDefinition $sig -Name CleanRoomNM -Namespace Win32 -PassThru
[void]$k::SetErrorMode(0x0001 -bor 0x0002 -bor 0x8000)

$saved = $env:PATH
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"   # bare Windows — NO dev tooling
$env:ACCT_PROBE = $probe
$env:ACCT_DATA_DIR = $data
try {
  $p = Start-Process -FilePath $Exe -PassThru -WindowStyle Hidden
  $ok = $p.WaitForExit(30000)
  if (-not $ok) { try { $p.Kill() } catch {}; $code = "TIMEOUT" } else { $code = $p.ExitCode }
} finally {
  $env:PATH = $saved
  Remove-Item Env:ACCT_PROBE, Env:ACCT_DATA_DIR -ErrorAction SilentlyContinue
}

function Check($name, $cond) {
  if ($cond) { Write-Host "  [PASS] $name" }
  else       { Write-Host "  [FAIL] $name"; $script:fail = 1 }
}

$script:fail = 0
Write-Host "== clean-room smoke (PATH = System32 only) =="
Write-Host "  exit code: $code"
$txt = if (Test-Path $probe) { Get-Content $probe -Raw } else { "" }

Check "process started + exited cleanly (0)"      ($code -eq 0)
Check "probe written (startup + engine load)"     ($txt.Length -gt 0)
Check "persistence/invoice load reachable"        ($txt -match "totalCount=\d+")
Check "translation catalog loaded (Arabic)"       ($txt -match "language=ar")
Check "RTL layout applied"                        ($txt -match "layoutDir=1")
Check "customer editor opened"                    ($txt -match "custEditorOpened=true")
Check "language list + endonyms present"          ($txt -match "langCount=3")

Write-Host ""
if ($script:fail -eq 0) { Write-Host "== CLEAN-ROOM: DEPLOYABLE ==" ; exit 0 }
else                    { Write-Host "== CLEAN-ROOM: FAILED ==" ;     exit 1 }
