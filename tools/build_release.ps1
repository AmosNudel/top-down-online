# Builds release client into dist/
$ErrorActionPreference = "Stop"
. "$PSScriptRoot/build_common.ps1"

$root = Split-Path -Parent $PSScriptRoot
$exeName = "TopDownSurvive"

Push-Location $root

Write-Host "Building release client..."
& $Make clean 2>$null
& $Make `
    RAYLIB_PATH=$RaylibPath `
    PROJECT_NAME=$exeName `
    "OBJS=$ClientObjs" `
    "ENET_CFLAGS=$EnetInclude" `
    "EXTRA_LIBS=-lws2_32" `
    BUILD_MODE=RELEASE

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$dist = Join-Path $root "dist"
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Path $dist | Out-Null

Copy-Item (Join-Path $root "$exeName.exe") $dist
foreach ($dir in @("characters", "map_tileset", "nature_tileset", "pickups", "sfx", "vfx")) {
    Copy-Item (Join-Path $root $dir) $dist -Recurse
}

Remove-Item (Join-Path $dist "game.log") -ErrorAction SilentlyContinue
Remove-Item (Join-Path $dist "generated_map.png") -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Done! Ship: $dist"
Write-Host "Also build server: .\tools\build_server.ps1"

Pop-Location
