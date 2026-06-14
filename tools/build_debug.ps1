# Builds a debug client .exe (connects to dedicated server by default).
$ErrorActionPreference = "Stop"
. "$PSScriptRoot/build_common.ps1"

$root = Split-Path -Parent $PSScriptRoot
$exeName = "TopDownSurviveOnline"

Push-Location $root

Write-Host "Building debug client..."
& $Make `
    RAYLIB_PATH=$RaylibPath `
    PROJECT_NAME=$exeName `
    "OBJS=$ClientObjs" `
    "ENET_CFLAGS=$EnetInclude" `
    "EXTRA_LIBS=-lws2_32" `
    BUILD_MODE=DEBUG

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Done! Terminal 1: .\tools\build_server.ps1 then dist-server\TopDownSurviveServer.exe"
Write-Host "       Terminal 2: .\TopDownSurviveOnline.exe"

Pop-Location
