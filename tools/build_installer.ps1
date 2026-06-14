# Builds dist/ then compiles TopDownSurvive_Setup.exe (requires Inno Setup 6).
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$iss = Join-Path $PSScriptRoot "installer.iss"

# 1) release build + dist folder
& (Join-Path $PSScriptRoot "build_release.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# 2) find Inno Setup compiler (ISCC.exe)
$isccCandidates = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
)
$iscc = $isccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $iscc) {
    Write-Host ""
    Write-Host "Inno Setup 6 is not installed."
    Write-Host "Download free from: https://jrsoftware.org/isdl.php"
    Write-Host "After installing, run this script again."
    Write-Host ""
    Write-Host "Your game is still ready to share as a zip of the dist\ folder:"
    Write-Host "  $root\dist"
    exit 1
}

Write-Host ""
Write-Host "Building installer..."
& $iscc $iss
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$setup = Join-Path $root "installer\top-down-survive-online_Setup.exe"
Write-Host ""
Write-Host "Done! Share this file with others:"
Write-Host "  $setup"
Write-Host ""
Write-Host "They run it, click through the wizard, and get Start Menu + optional desktop icon."
