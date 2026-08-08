# i18n-extract.ps1 — scripted translation extraction (replaces manual lupdate).
#
# Runs Qt lupdate over all Quick QML to refresh the .ts catalogs. Handles the
# Windows UAC installer-detection quirk that blocks lupdate.exe ("requires
# elevation") by forcing the RunAsInvoker compatibility layer.
#
# Usage:  pwsh tools/i18n-extract.ps1   (or:  powershell -File tools\i18n-extract.ps1)

$ErrorActionPreference = "Stop"
$root   = Split-Path -Parent $PSScriptRoot
$qtBin  = "C:\msys64\ucrt64\bin"
$lupdate = Join-Path $qtBin "lupdate.exe"

if (-not (Test-Path $lupdate)) {
    Write-Error "lupdate not found at $lupdate — install mingw-w64-ucrt-x86_64-qt6-tools"
    exit 1
}

$env:PATH = "$qtBin;" + $env:PATH
# Bypass UAC auto-elevation (the embedded manifest trips installer detection).
$env:__COMPAT_LAYER = "RunAsInvoker"

$ts = @("quick/i18n/app_en.ts", "quick/i18n/app_fr.ts", "quick/i18n/app_ar.ts")

# Source roots to scan. QML is the primary UI surface; a small set of C++ ViewModels/models
# also expose user-facing text via tr() (status/diagnostic/label strings that cannot be pushed
# into QML, e.g. combo-option labels and model DisplayRole text). List those files explicitly so
# their tr() strings are extracted + translated too. (Test/harness .cpp are intentionally omitted.)
$sources = @(
    "quick/qml",
    "quick/DiagnosticsViewModel.cpp",
    "quick/BackupViewModel.cpp",
    "quick/SettingsViewModel.cpp",
    "quick/LedgerEntriesModel.cpp",
    "quick/ExpenseEditorViewModel.cpp"
)
Push-Location $root
try {
    Write-Host "Extracting strings from $($sources -join ', ') -> $($ts -join ', ')"
    & $lupdate $sources -ts $ts
    if ($LASTEXITCODE -ne 0) { Write-Error "lupdate failed (exit $LASTEXITCODE)"; exit 1 }
    Write-Host "Done. Review new 'unfinished' entries in app_fr.ts / app_ar.ts and translate them,"
    Write-Host "then run tools/i18n-check.sh and rebuild (lrelease compiles the .qm)."
} finally {
    Pop-Location
}
