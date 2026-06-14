# Builds the raylib game for WebAssembly and deploys to a Next.js public folder.
param(
    [string]$DeployPath = "C:\Users\nudel\Desktop\Portfolio\portfolio_next_frontend\public\top-down-survive"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$w64devkit = "C:/raylib/w64devkit/bin"
$make = Join-Path $w64devkit "mingw32-make.exe"
$raylibSrc = "C:/raylib/raylib/src"
$exeName = "TopDownSurvive"
$webResources = Join-Path $root "build\web\resources"
$assetDirs = @("characters", "map_tileset", "nature_tileset", "pickups", "sfx", "vfx")

function Find-EmsdkEnv {
    $candidates = @(
        "C:\raylib\emsdk\emsdk_env.ps1",
        "C:\emsdk\emsdk_env.ps1",
        (Join-Path $env:USERPROFILE "emsdk\emsdk_env.ps1")
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }
    return $null
}

# 1) Activate Emscripten
$emsdkEnv = Find-EmsdkEnv
if (-not $emsdkEnv) {
    Write-Host ""
    Write-Host "Emscripten (emsdk) is not installed."
    Write-Host ""
    Write-Host "One-time setup:"
    Write-Host "  git clone https://github.com/emscripten-core/emsdk.git C:\raylib\emsdk"
    Write-Host "  cd C:\raylib\emsdk"
    Write-Host "  .\emsdk install latest"
    Write-Host "  .\emsdk activate latest"
    Write-Host ""
    Write-Host "Then run this script again."
    exit 1
}

Write-Host "Activating Emscripten..."
. $emsdkEnv

# emsdk replaces PATH; keep w64devkit so mingw32-make stays available
$env:PATH = "$w64devkit;$env:PATH"

if (-not (Get-Command emcc -ErrorAction SilentlyContinue)) {
    Write-Host "emcc not found after emsdk activation."
    exit 1
}

# 2) Build raylib for web if needed
$raylibWeb = Join-Path $raylibSrc "libraylib.web.a"
if (-not (Test-Path $raylibWeb)) {
    Write-Host "Building raylib for web (first time only)..."
    Push-Location $raylibSrc
    & $make PLATFORM=PLATFORM_WEB
    if ($LASTEXITCODE -ne 0) { Pop-Location; exit $LASTEXITCODE }
    Pop-Location
}

# 3) Stage assets for --preload-file
Write-Host "Staging web assets..."
if (Test-Path $webResources) { Remove-Item $webResources -Recurse -Force }
New-Item -ItemType Directory -Path $webResources | Out-Null
foreach ($dir in $assetDirs) {
    $src = Join-Path $root $dir
    if (-not (Test-Path $src)) {
        Write-Host "Missing asset folder: $src"
        exit 1
    }
    Copy-Item $src (Join-Path $webResources $dir) -Recurse
}

# 4) Compile game to HTML/WASM
Write-Host "Compiling web build..."
Push-Location $root
& $make clean `
    RAYLIB_PATH=C:/raylib/raylib `
    PLATFORM=PLATFORM_WEB `
    PROJECT_NAME=$exeName `
    "MAKE=$make"
& $make $exeName `
    RAYLIB_PATH=C:/raylib/raylib `
    PLATFORM=PLATFORM_WEB `
    PROJECT_NAME=$exeName `
    "OBJS=$WebObjs" `
    BUILD_MODE=RELEASE `
    "MAKE=$make"
$buildOk = ($LASTEXITCODE -eq 0)
Pop-Location
if (-not $buildOk) { exit 1 }

# 5) Deploy to Next.js public folder
Write-Host "Deploying to $DeployPath ..."
if (Test-Path $DeployPath) { Remove-Item $DeployPath -Recurse -Force }
New-Item -ItemType Directory -Path $DeployPath | Out-Null

Copy-Item (Join-Path $root "$exeName.html") (Join-Path $DeployPath "index.html")
Copy-Item (Join-Path $root "$exeName.js") $DeployPath
Copy-Item (Join-Path $root "$exeName.wasm") $DeployPath
$dataFile = Join-Path $root "$exeName.data"
if (Test-Path $dataFile) { Copy-Item $dataFile $DeployPath }

Write-Host ""
Write-Host "Web build ready!"
Write-Host "  $DeployPath\index.html"
Write-Host ""
Write-Host "In Next.js, embed like your Unity game:"
Write-Host '  <iframe src="/top-down-survive/index.html" className="absolute inset-0 h-full w-full border-0" />'
Write-Host ""
Write-Host "Test locally:  cd portfolio_next_frontend && npm run dev"
