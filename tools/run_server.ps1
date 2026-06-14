# Starts the dedicated server from dist-server/
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$serverExe = Join-Path $root "dist-server\TopDownSurviveServer.exe"
$port = 27015
$tcpPort = 27016

if ($args.Count -ge 1) { $port = $args[0] }
if ($args.Count -ge 2) { $tcpPort = $args[1] }

if (-not (Test-Path $serverExe)) {
    Write-Host "Server not built. Run: .\tools\build_server.ps1"
    exit 1
}

Get-Process -Name TopDownSurviveServer -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

Push-Location (Split-Path $serverExe)
& $serverExe --port $port --tcp-port $tcpPort
Pop-Location
