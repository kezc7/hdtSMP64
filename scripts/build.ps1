[CmdletBinding()]
param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [switch]$RunTests
)

$ErrorActionPreference = 'Stop'

function Stop-Build([string]$Message) {
    Write-Error $Message
    exit 1
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

$cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
if (-not $cmake) {
    Stop-Build 'CMake was not found on PATH. Install CMake 3.22 or newer and reopen PowerShell.'
}

$vswhereCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
    (Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio\Installer\vswhere.exe')
)
$vswhere = $vswhereCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ($vswhere) {
    $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsInstall) {
        Stop-Build 'Visual Studio 2022 with the MSVC v143 C++ workload was not found.'
    }
    Write-Host "Visual Studio: $vsInstall"
} elseif (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    Stop-Build 'Visual Studio/MSVC was not found. Install the Desktop development with C++ workload or run from a VS 2022 Developer PowerShell.'
}

if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    Stop-Build 'VCPKG_ROOT is not set. Clone vcpkg outside this repository, bootstrap it, then set $env:VCPKG_ROOT or pass -VcpkgRoot.'
}

$vcpkgRootResolved = (Resolve-Path -LiteralPath $VcpkgRoot -ErrorAction SilentlyContinue)
if (-not $vcpkgRootResolved) {
    Stop-Build "vcpkg directory does not exist: $VcpkgRoot"
}
$vcpkgRoot = $vcpkgRootResolved.Path
$toolchain = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
$vcpkgExe = Join-Path $vcpkgRoot 'vcpkg.exe'
if (-not (Test-Path -LiteralPath $toolchain) -or -not (Test-Path -LiteralPath $vcpkgExe)) {
    Stop-Build "vcpkg is not bootstrapped at $vcpkgRoot. Run bootstrap-vcpkg.bat there."
}
$env:VCPKG_ROOT = $vcpkgRoot

$preset = 'vs2022-windows-ae-1170'
$buildPreset = 'windows-ae-1170-release'
$configureArgs = @('--preset', $preset)
if ($RunTests) {
    $configureArgs += '-DBUILD_VALIDATOR_TESTS=ON'
    $configureArgs += '-DBUILD_PATTERN_TESTS=ON'
    $configureArgs += '-DBUILD_CONFIG_TESTS=ON'
}

Push-Location $repoRoot
try {
    Write-Host "Configuring $preset..."
    & $cmake.Source @configureArgs
    if ($LASTEXITCODE -ne 0) {
        Stop-Build "CMake configure failed with exit code $LASTEXITCODE."
    }

    Write-Host "Building $buildPreset..."
    & $cmake.Source '--build' '--preset' $buildPreset
    if ($LASTEXITCODE -ne 0) {
        Stop-Build "CMake build failed with exit code $LASTEXITCODE."
    }

    if ($RunTests) {
        Write-Host 'Running tests...'
        & ctest.exe '--test-dir' 'out/build/vs2022-windows-ae-1170' '-C' 'Release' '--output-on-failure'
        if ($LASTEXITCODE -ne 0) {
            Stop-Build "CTest failed with exit code $LASTEXITCODE."
        }
    }
} finally {
    Pop-Location
}

$artifactRoot = Join-Path $repoRoot 'out\build\vs2022-windows-ae-1170'
Write-Host "DLL: $artifactRoot\plugins\SKSE\Plugins\hdtsmp64.dll"
Write-Host "PDB: $artifactRoot\plugins\SKSE\Plugins\hdtsmp64.pdb"
