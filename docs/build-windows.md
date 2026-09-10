# Windows build

This repository builds the native `hdtsmp64.dll` SKSE plugin with CMake, Visual Studio 2022, and vcpkg manifest mode. The repository pins the vcpkg dependency baseline in `vcpkg.json` and supplies overlay ports/triplets in `cmake/`.

The `vs2022-windows-ae-1170` preset compiles the Skyrim AE runtime configuration used by Skyrim AE 1.6.1170. The runtime version is selected by Address Library at runtime; the preset does not bundle Skyrim or Address Library files.

## Clean checkout

Open **Developer PowerShell for VS 2022** or a regular PowerShell with Visual Studio 2022 Desktop development with C++ installed:

```powershell
git clone https://github.com/kezc7/hdtSMP64.git
Set-Location hdtSMP64
git switch dev
git submodule update --init --recursive
```

Install CMake 3.22 or newer and Visual Studio 2022 with:

- Desktop development with C++
- MSVC v143 build tools
- Windows 10 or 11 SDK

Install vcpkg outside the repository. Any stable path is supported; `C:\src\vcpkg` is only an example:

```powershell
New-Item -ItemType Directory -Force C:\src | Out-Null
git clone https://github.com/microsoft/vcpkg.git C:\src\vcpkg
$env:VCPKG_ROOT = 'C:\src\vcpkg'
& "$env:VCPKG_ROOT\bootstrap-vcpkg.bat"
```

For future shells, set `VCPKG_ROOT` persistently if desired:

```powershell
[Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\src\vcpkg', 'User')
```

Do not clone vcpkg into this repository. The CMake preset reads `VCPKG_ROOT` and uses the repository's manifest, overlay ports, and overlay triplets automatically.

## Configure and build Release

From the repository root:

```powershell
cmake --preset vs2022-windows-ae-1170
cmake --build --preset windows-ae-1170-release
```

The first configure installs the manifest dependencies and may take time. The build is x64 Release and uses the v143 toolset.

The resulting files are available in both locations:

```text
out/build/vs2022-windows-ae-1170/src/Release/hdtsmp64.dll
out/build/vs2022-windows-ae-1170/src/Release/hdtsmp64.pdb
out/build/vs2022-windows-ae-1170/plugins/SKSE/Plugins/hdtsmp64.dll
out/build/vs2022-windows-ae-1170/plugins/SKSE/Plugins/hdtsmp64.pdb
```

The `plugins/SKSE/Plugins/` copy is the deployment-shaped output used by the CI artifact. `CompiledPluginsPath` defaults to this build-local directory, so no extra shell variable is required. It can still be overridden for a local game installation or CI.

## Tests

The normal CI configuration builds the validator tests. To enable them locally, configure with:

```powershell
cmake --preset vs2022-windows-ae-1170 -DBUILD_VALIDATOR_TESTS=ON -DBUILD_PATTERN_TESTS=ON -DBUILD_CONFIG_TESTS=ON
cmake --build --preset windows-ae-1170-release
ctest --test-dir out/build/vs2022-windows-ae-1170 -C Release --output-on-failure
```

Alternatively, `scripts/build.ps1` performs the tool checks, configures the AE preset, builds Release, and prints the artifact paths:

```powershell
.\scripts\build.ps1
```

Use `-VcpkgRoot D:\tools\vcpkg` when the vcpkg installation is not exposed through `VCPKG_ROOT`.
