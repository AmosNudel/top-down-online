# Builds TopDownSurviveServer.exe (headless dedicated server).
$ErrorActionPreference = "Stop"
. "$PSScriptRoot/build_common.ps1"

$root = Split-Path -Parent $PSScriptRoot
$exeName = "TopDownSurviveServer"

Push-Location $root

Write-Host "Building dedicated server..."
& $Make clean 2>$null
& $Make `
    RAYLIB_PATH=$RaylibPath `
    PROJECT_NAME=$exeName `
    "OBJS=$ServerObjs" `
    "ENET_CFLAGS=$EnetInclude" `
    "EXTRA_LIBS=-lws2_32" `
    BUILD_MODE=RELEASE

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Get-Process -Name $exeName -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

$dist = Join-Path $root "dist-server"
if (-not (Test-Path $dist)) {
    New-Item -ItemType Directory -Path $dist | Out-Null
}

Copy-Item (Join-Path $root "$exeName.exe") (Join-Path $dist "$exeName.exe") -Force
foreach ($dir in @("characters", "map_tileset", "nature_tileset", "pickups", "sfx", "vfx")) {
    $src = Join-Path $root $dir
    $dst = Join-Path $dist $dir
    if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
    Copy-Item $src $dst -Recurse
}

Write-Host ""
Write-Host "Done! Stop any running server, then:"
Write-Host "  cd dist-server"
Write-Host "  .\TopDownSurviveServer.exe --port 27015 --tcp-port 27016"
Write-Host ""
Write-Host "For Railway/web clients, run the relay:"
Write-Host "  cd relay && npm install && npm start"
Write-Host "  WebSocket: ws://127.0.0.1:8080/game"
Write-Host ""
Write-Host "Check server.log for 'wall-clock-ticks v3' and 'Countdown ticking'."
Write-Host ""
Write-Host "Building debug client..."
& "$PSScriptRoot/build_debug.ps1"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Pop-Location
