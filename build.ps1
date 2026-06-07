# Build and run script for SFML3 project
# Usage:
#   .\build.ps1          - build Debug and run
#   .\build.ps1 release  - build Release and run
#   .\build.ps1 build    - build only (no run)
#   .\build.ps1 clean    - delete build folder and rebuild

param(
    [string]$Mode = "debug"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$BuildDir = "$ProjectRoot\build"
$Config = if ($Mode -eq "release") { "Release" } else { "Debug" }

# Clean
if ($Mode -eq "clean") {
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
        Write-Host "Build directory cleaned." -ForegroundColor Yellow
    }
    $Config = "Debug"
}

# Configure (only if needed)
if (-not (Test-Path "$BuildDir\CMakeCache.txt")) {
    Write-Host "Configuring CMake..." -ForegroundColor Cyan
    cmake -S $ProjectRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) { Write-Host "CMake configure failed!" -ForegroundColor Red; exit 1 }
}

# Build
Write-Host "Building ($Config)..." -ForegroundColor Cyan
cmake --build $BuildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed!" -ForegroundColor Red; exit 1 }

Write-Host "Build OK!" -ForegroundColor Green

# Run (unless build-only mode)
if ($Mode -ne "build") {
    $Exe = "$BuildDir\$Config\SFML3.exe"
    if (Test-Path $Exe) {
        Write-Host "Running $Config..." -ForegroundColor Cyan
        Push-Location $ProjectRoot
        & $Exe
        Pop-Location
    } else {
        Write-Host "Executable not found at $Exe" -ForegroundColor Red
    }
}
