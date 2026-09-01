# build rp2040 firmware on windows
# downloads a prebuilt picotool so the sdk never builds host tools from source
# usage: ./setup-windows.ps1 [target]   (no target = build all)

param([string]$Target = "", [switch]$Clean)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $here "build"
$tools = Join-Path $here ".tools"
$picotoolDir = Join-Path $tools "picotool"
$ptUrl = "https://github.com/raspberrypi/pico-sdk-tools/releases/download/v2.3.0-1/picotool-2.3.0-x64-win.zip"

function Need($cmd, $pkg) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        Write-Warning "$cmd not found, installing $pkg..."
        winget install --id $pkg -e --accept-source-agreements --accept-package-agreements
    }
}
Need cmake Kitware.CMake
Need ninja Ninja-build.Ninja
Need just Casey.Just
Need gh GitHub.cli
Need arm-none-eabi-gcc Arm.GnuArmEmbeddedToolchain

# winget writes PATH to the registry, not this session; refresh so fresh installs are usable now
$env:PATH = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")

if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

if (-not (Test-Path (Join-Path $picotoolDir "picotoolConfig.cmake"))) {
    Write-Host "downloading prebuilt picotool..."
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    $zip = Join-Path $tools "picotool.zip"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $ptUrl -OutFile $zip -UseBasicParsing
    Expand-Archive -Path $zip -DestinationPath $tools -Force
    Remove-Item $zip
}

$picotoolDirFwd = $picotoolDir -replace '\\', '/'

# cmake writes status messages to stderr; under ErrorActionPreference=Stop
# powershell 5.1 turns those into terminating errors. exit codes are checked below.
$ErrorActionPreference = "Continue"
cmake -S $here -B $build -G Ninja "-Dpicotool_DIR=$picotoolDirFwd"
if ($LASTEXITCODE -ne 0) { throw "configure failed" }

if ($Target) { cmake --build $build --target $Target }
else { cmake --build $build }
if ($LASTEXITCODE -ne 0) { throw "build failed" }

Write-Host "done. uf2 files: build\src\<project>\<project>.uf2"
