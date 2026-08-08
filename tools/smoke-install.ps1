# smoke-install.ps1 -- end-to-end Windows release smoke test.
#
#   fresh environment  ->  INSTALL  ->  LAUNCH  ->  create EMPTY COMPANY  ->  ACTIVATE LICENSE
#                       ->  (uninstall preserves the user's data)
#
# This exercises the SHIPPING artifact the way a real customer would, on this machine, with the
# dev toolchain removed from PATH (System32 only) so it proves the installed tree is self-contained.
# It touches NO application source -- it drives the app only through the env-var harness the engine
# already exposes (ACCT_PROBE / ACCT_DATA_DIR / ACCT_CONFIG_DIR) and the on-disk contract the
# LicenseManager already uses (a license.key file in the config dir == "Enter license key").
#
# -- What each phase asserts ------------------------------------------------------------------------
#   INSTALL   The built Occountant-*-Setup.exe installs silently into an isolated prefix and lays
#             AccountingQuick.exe + its full Qt/MinGW runtime closure there. When Inno Setup is not
#             installed (so no installer exists), it DEGRADES to the staged runtime tree
#             (dist\Occountant) copied into a scratch "install" dir -- the byte-identical payload the
#             installer would have shipped -- and clearly says so. (Mirrors tools/release.sh, which
#             also skips the installer when iscc is absent.)
#   LAUNCH +  Runs the INSTALLED exe with a clean PATH against a fresh, empty data dir. A first
#   COMPANY   launch provisions an empty company (fresh books, 0 invoices) and the LicenseManager
#             auto-issues a 30-day Trial. Asserted from the ACCT_PROBE readout + the health line the
#             app writes to <data>\logs\occountant.log.
#   ACTIVATE  Mints a vendor Business license with the developer-only license_gen (Ed25519 private
#             key; never shipped), installs it exactly as the in-app "Enter license key" does -- by
#             writing the OCCLIC- token to <config>\license.key -- and relaunches. The installed
#             (release) binary VERIFIES the vendor signature with only its embedded PUBLIC key and
#             flips from Trial to Business. If no signing generator / key is available, activation is
#             reported SKIPPED (a warning, not a failure) unless -StrictLicense is given.
#   UNINSTALL Removes the install tree and asserts the data dir (books) and config dir (license.key)
#             are untouched -- an uninstall must never destroy a business's accounting records.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\smoke-install.ps1
#   powershell ... -File tools\smoke-install.ps1 -Setup dist\release\1.0.0-stable\Occountant-Setup.exe
#   powershell ... -File tools\smoke-install.ps1 -LicenseKey OCCLIC-... -StrictLicense
#   powershell ... -File tools\smoke-install.ps1 -IntoProgramFiles          # real elevated install
#
# Exit 0 = all required phases passed (license activation may be SKIPPED with a warning).
[CmdletBinding()]
param(
  [string]$Setup      = "",        # Occountant-*-Setup.exe; auto-discovered under dist\ if empty
  [string]$Stage      = "",        # staged runtime tree used when no installer exists (default dist\Occountant)
  [string]$LicenseKey = "",        # pre-minted OCCLIC- key; if empty, mint one with license_gen
  [string]$LicenseGen = "",        # path to a signing-build license_gen.exe; auto-discovered
  [switch]$IntoProgramFiles,       # do a real (elevated) Program Files install instead of a scratch prefix
  [switch]$StrictLicense,          # FAIL (not warn) if license activation cannot be exercised
  [switch]$KeepArtifacts,          # keep the scratch working dir for inspection
  [int]   $TimeoutSec = 60
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$fail = 0
$script:checks = 0

function Step($t) { Write-Host ""; Write-Host "======== $t ========" }
function Info($t) { Write-Host "  $t" }
function Warn($t) { Write-Host "  ! $t" -ForegroundColor Yellow }
function Check($name, $cond) {
  $script:checks++
  if ($cond) { Write-Host "  [PASS] $name" -ForegroundColor Green }
  else       { Write-Host "  [FAIL] $name" -ForegroundColor Red; $script:fail = 1 }
}

# Suppress hard-error dialogs (a missing DLL / crash must never block an automated run).
$sig = '[DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint mode);'
$nm  = Add-Type -MemberDefinition $sig -Name SmokeNM -Namespace Win32 -PassThru
[void]$nm::SetErrorMode(0x0001 -bor 0x0002 -bor 0x8000)

# Isolated, self-cleaning working area (install prefix + fresh data + fresh config all live here).
$work = Join-Path $env:TEMP ("occountant-smoke-" + [Guid]::NewGuid().ToString("N").Substring(0,8))
New-Item -ItemType Directory -Force -Path $work | Out-Null
$probe = Join-Path $work "probe.txt"
Info "working dir: $work"

# -- Launch the INSTALLED exe headlessly with a CLEAN PATH, return exit code + probe + health -------
function Invoke-AppProbe($exe, $data, $config) {
  Remove-Item $probe -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $data, $config | Out-Null
  # Clear the rolling log so we read THIS launch's health line, not one appended by an earlier launch
  # against the same (persistent) data dir. Books (*.dat) are left untouched.
  Remove-Item (Join-Path $data "logs\occountant*.log") -ErrorAction SilentlyContinue
  $savedPath = $env:PATH
  $env:PATH            = "$env:SystemRoot\System32;$env:SystemRoot"   # bare Windows -- no MSYS2/Qt/ucrt64
  $env:ACCT_PROBE      = $probe
  $env:ACCT_DATA_DIR   = $data
  $env:ACCT_CONFIG_DIR = $config
  try {
    $p  = Start-Process -FilePath $exe -PassThru -WindowStyle Hidden
    $ok = $p.WaitForExit($TimeoutSec * 1000)
    if (-not $ok) { try { $p.Kill() } catch {}; $code = -999 } else { $code = $p.ExitCode }
  } finally {
    $env:PATH = $savedPath
    Remove-Item Env:ACCT_PROBE, Env:ACCT_DATA_DIR, Env:ACCT_CONFIG_DIR -ErrorAction SilentlyContinue
  }
  $probeTxt = if (Test-Path $probe) { Get-Content $probe -Raw } else { "" }
  $logPath  = Join-Path $data "logs\occountant.log"
  $health   = if (Test-Path $logPath) { Get-Content $logPath -Raw } else { "" }
  # The app writes one line: "license:  <State> (<Edition>)  valid=<yes|no>  daysLeft=<n>"
  # Take the LAST such line, so we always reflect the most recent launch's state.
  $edition = ""; $valid = ""
  $ms = [regex]::Matches($health, 'license:\s+\S+\s+\(([^)]+)\)\s+valid=(\w+)')
  if ($ms.Count -gt 0) { $m = $ms[$ms.Count-1]; $edition = $m.Groups[1].Value; $valid = $m.Groups[2].Value }
  [pscustomobject]@{ Code = $code; Probe = $probeTxt; Health = $health; Edition = $edition; Valid = $valid }
}

# -- PHASE 1 -- INSTALL -----------------------------------------------------------------------------
Step "1) INSTALL"
if ([string]::IsNullOrEmpty($Setup)) {
  $found = Get-ChildItem -Path (Join-Path $repo "dist") -Recurse -Filter "Occountant*Setup.exe" -ErrorAction SilentlyContinue |
           Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if ($found) { $Setup = $found.FullName }
}
$installDir  = ""
$uninstaller = ""
$installerMode = $false

if ((-not [string]::IsNullOrEmpty($Setup)) -and (Test-Path $Setup)) {
  $installerMode = $true
  $Setup = (Resolve-Path $Setup).Path
  Info "installer: $Setup"
  if ($IntoProgramFiles) {
    $installDir = Join-Path $env:ProgramFiles "Occountant"
    $args = @("/VERYSILENT","/SUPPRESSMSGBOXES","/NORESTART","/LOG=$work\install.log")
  } else {
    # Install into a scratch prefix (hermetic, non-destructive) with /DIR. The installer still runs
    # elevated (it writes {app} + HKLM), so on an already-elevated session -- e.g. a CI runner -- this
    # is silent; on an interactive non-admin box it raises one UAC prompt, then proceeds. This keeps
    # the SHIPPED installer untouched (no per-user override); it just redirects the install location.
    $installDir = Join-Path $work "install"
    $args = @("/VERYSILENT","/SUPPRESSMSGBOXES","/NORESTART","/DIR=$installDir","/LOG=$work\install.log")
  }
  $p = Start-Process -FilePath $Setup -ArgumentList $args -Wait -PassThru
  Check "installer exited 0 (silent install)" ($p.ExitCode -eq 0)
  $uninstaller = Join-Path $installDir "unins000.exe"
} else {
  Warn "Inno Setup installer not found -- DEGRADING to the staged runtime tree (install-equivalent payload)."
  Warn "Install Inno Setup 6 and run tools/release.sh to exercise the real Occountant-Setup.exe."
  if ([string]::IsNullOrEmpty($Stage)) { $Stage = Join-Path $repo "dist\Occountant" }
  if (-not (Test-Path (Join-Path $Stage "AccountingQuick.exe"))) {
    Write-Host "  [FAIL] no installer AND no staged tree at $Stage (run tools/stage-runtime.sh first)" -ForegroundColor Red
    if (-not $KeepArtifacts) { Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue }
    exit 1
  }
  $installDir = Join-Path $work "install"
  New-Item -ItemType Directory -Force -Path $installDir | Out-Null
  Info "staging payload -> $installDir (simulates the installed location)"
  Copy-Item -Path (Join-Path $Stage "*") -Destination $installDir -Recurse -Force
}

$installedExe = Join-Path $installDir "AccountingQuick.exe"
Check "installed AccountingQuick.exe present under the install prefix" (Test-Path $installedExe)
if (-not (Test-Path $installedExe)) {
  if (-not $KeepArtifacts) { Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue }
  exit 1
}

# -- PHASE 2 -- LAUNCH + CREATE EMPTY COMPANY -------------------------------------------------------
Step "2) LAUNCH + CREATE EMPTY COMPANY (fresh books, clean PATH)"
$data   = Join-Path $work "data"
$config = Join-Path $work "config"
$r1 = Invoke-AppProbe $installedExe $data $config
Info "exit=$($r1.Code)  license=$($r1.Edition)/$($r1.Valid)"
Check "installed app launched + exited cleanly (0)"          ($r1.Code -eq 0)
Check "engine loaded (ACCT_PROBE readout produced)"          ($r1.Probe.Length -gt 0)
$totalCount = -1
$mt = [regex]::Match($r1.Probe, 'totalCount=(\d+)')
if ($mt.Success) { $totalCount = [int]$mt.Groups[1].Value }
Check "empty company provisioned (0 invoices on first run)"  ($totalCount -eq 0)
$datFiles = @(Get-ChildItem -Path $data -Filter *.dat -Recurse -ErrorAction SilentlyContinue)
Check "fresh books written to the data dir (*.dat created)"  ($datFiles.Count -ge 1)
Check "first-run Trial license auto-issued + valid"          (($r1.Edition -eq "Trial") -and ($r1.Valid -eq "yes"))

# -- PHASE 3 -- ACTIVATE LICENSE --------------------------------------------------------------------
Step "3) ACTIVATE LICENSE (vendor Business key)"
if ([string]::IsNullOrEmpty($LicenseKey)) {
  if ([string]::IsNullOrEmpty($LicenseGen)) {
    foreach ($cand in @("build\license_gen_dist\license_gen.exe",
                        "build-sign\license_gen_dist\license_gen.exe",
                        "build\license_gen.exe")) {
      $full = Join-Path $repo $cand
      if (Test-Path $full) { $LicenseGen = $full; break }
    }
  }
  if ((-not [string]::IsNullOrEmpty($LicenseGen)) -and (Test-Path $LicenseGen)) {
    $LicenseGen = (Resolve-Path $LicenseGen).Path
    Info "minting a Business license with $([IO.Path]::GetFileName($LicenseGen))"
    $expiry = (Get-Date).AddYears(1).ToString('yyyy-MM-dd')
    $out = & $LicenseGen --name "Smoke Test Co" --plan business --expires $expiry
    $LicenseKey = ($out | Where-Object { $_ -match '^OCCLIC-' } | Select-Object -First 1)
  }
}

if ([string]::IsNullOrEmpty($LicenseKey)) {
  $msg = "no vendor signing key/tool available - license activation NOT exercised. " +
         "Pass -LicenseKey OCCLIC-..., or build license_gen with -DACCT_DEV_SIGNING=ON."
  if ($StrictLicense) { Check "license activation exercised" $false; Warn $msg }
  else                { Warn "ACTIVATION SKIPPED: $msg" }
} else {
  Info "activating: $($LicenseKey.Substring(0, [Math]::Min(28,$LicenseKey.Length)))..."
  # Persist the key exactly as LicenseManager::activate() does: the raw token in <config>\license.key.
  # UTF-8 with NO BOM -- a BOM would corrupt the base64url payload the verifier reads.
  $keyPath = Join-Path $config "license.key"
  [System.IO.File]::WriteAllText($keyPath, $LicenseKey.Trim(), (New-Object System.Text.UTF8Encoding($false)))
  $r2 = Invoke-AppProbe $installedExe $data $config
  Info "exit=$($r2.Code)  license=$($r2.Edition)/$($r2.Valid)"
  Check "installed app still launches after activation (0)"   ($r2.Code -eq 0)
  Check "license verified + upgraded to Business"             ($r2.Edition -eq "Business")
  Check "activated license reports valid=yes"                 ($r2.Valid -eq "yes")
  Check "no longer running on the Trial license"              ($r2.Edition -ne "Trial")
}

# -- PHASE 4 -- UNINSTALL PRESERVES USER DATA -------------------------------------------------------
Step "4) UNINSTALL preserves user data"
$datBefore = @(Get-ChildItem -Path $data -Filter *.dat -Recurse -ErrorAction SilentlyContinue).Count
$keyBefore = Test-Path (Join-Path $config "license.key")
if ($installerMode -and (Test-Path $uninstaller)) {
  Info "running uninstaller: $uninstaller"
  $u = Start-Process -FilePath $uninstaller -ArgumentList @("/VERYSILENT","/SUPPRESSMSGBOXES","/NORESTART") -Wait -PassThru
  Check "uninstaller exited 0" ($u.ExitCode -eq 0)
  Start-Sleep -Milliseconds 500   # unins hands off to a temp copy of itself; let it finish removing {app}
} else {
  Info "simulating uninstall: removing the install tree (data/config live elsewhere, untouched)"
  Remove-Item $installDir -Recurse -Force -ErrorAction SilentlyContinue
}
Check "install tree removed (binaries gone)"                 (-not (Test-Path $installedExe))
$datAfter = @(Get-ChildItem -Path $data -Filter *.dat -Recurse -ErrorAction SilentlyContinue).Count
Check "accounting data preserved across uninstall (*.dat)"   (($datAfter -ge 1) -and ($datAfter -eq $datBefore))
if ($keyBefore) {
  Check "activated license preserved across uninstall"       (Test-Path (Join-Path $config "license.key"))
}

# -- Summary ----------------------------------------------------------------------------------------
Step "SMOKE RESULT"
if (-not $KeepArtifacts) { Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue }
else { Info "artifacts kept at $work" }
Info "$script:checks checks run"
if ($fail -eq 0) { Write-Host "== SMOKE: PASSED ==" -ForegroundColor Green; exit 0 }
else             { Write-Host "== SMOKE: FAILED ==" -ForegroundColor Red;   exit 1 }
