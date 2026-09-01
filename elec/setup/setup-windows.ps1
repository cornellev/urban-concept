# set up the windows toolchain for building rp2040 firmware
# installs any missing tools via winget and downloads a prebuilt picotool
# after this, build with: just elec build-all   (or: just elec build <project>)

$ErrorActionPreference = "Stop"
$elec = Split-Path -Parent $PSScriptRoot
$tools = Join-Path $elec ".tools"
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
Need arm-none-eabi-gcc Arm.ArmGnuToolchain

# winget writes PATH to the registry, not this session; refresh so fresh installs are usable now
$env:PATH = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")

if (-not (Test-Path (Join-Path $picotoolDir "picotoolConfig.cmake"))) {
    Write-Host "downloading prebuilt picotool..."
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    $zip = Join-Path $tools "picotool.zip"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $ptUrl -OutFile $zip -UseBasicParsing
    Expand-Archive -Path $zip -DestinationPath $tools -Force
    Remove-Item $zip
}

Write-Host "setup complete. build with: just elec build-all"
