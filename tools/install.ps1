# Installs the game to AppData and puts a shortcut on the desktop.
# You only interact with the shortcut — asset folders stay in AppData.
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $root "dist"
$exeName = "TopDownSurvive"
$installDir = Join-Path $env:LOCALAPPDATA "TopDownSurvive"
$desktopShortcut = Join-Path ([Environment]::GetFolderPath("Desktop")) "Top Down Survive.lnk"

if (-not (Test-Path (Join-Path $dist "$exeName.exe"))) {
    Write-Host "dist\$exeName.exe not found. Run tools\build_release.ps1 first."
    exit 1
}

Write-Host "Installing to $installDir ..."
if (Test-Path $installDir) { Remove-Item $installDir -Recurse -Force }
New-Item -ItemType Directory -Path $installDir | Out-Null

Copy-Item (Join-Path $dist "$exeName.exe") $installDir
Copy-Item (Join-Path $dist "characters") $installDir -Recurse
Copy-Item (Join-Path $dist "map_tileset") $installDir -Recurse
Copy-Item (Join-Path $dist "nature_tileset") $installDir -Recurse
Copy-Item (Join-Path $dist "pickups") $installDir -Recurse
Copy-Item (Join-Path $dist "sfx") $installDir -Recurse
Copy-Item (Join-Path $dist "vfx") $installDir -Recurse

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($desktopShortcut)
$shortcut.TargetPath = Join-Path $installDir "$exeName.exe"
$shortcut.WorkingDirectory = $installDir
$shortcut.Description = "Top Down Survive"
$shortcut.Save()

Write-Host ""
Write-Host "Done!"
Write-Host "  Game files: $installDir"
Write-Host "  Desktop shortcut: $desktopShortcut"
Write-Host ""
Write-Host "Double-click 'Top Down Survive' on your desktop to play."
