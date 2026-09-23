# set up the windows toolchain for building rp2040 firmware
# installs any missing tools via winget and downloads a prebuilt picotool and arm-gcc
# after this, build with: just elec build   (or one target: just elec build-target <project>)

$ErrorActionPreference = "Stop"
$elec = Split-Path -Parent $PSScriptRoot
$tools = Join-Path $elec ".tools"
$picotoolDir = Join-Path $tools "picotool"
$ptUrl = "https://github.com/raspberrypi/pico-sdk-tools/releases/download/v2.3.0-1/picotool-2.3.0-x64-win.zip"
$ptSha = "52d63c5fb4cc19f95b62bb6c945d2bcb772ec426c7676abf585437849c81c870"
# winget's Arm.GnuArmEmbeddedToolchain stops at 10.3, so fetch the pinned release directly
$armDir = Join-Path $tools "arm-gnu-toolchain"
$armUrl = "https://gitlab.arm.com/api/v4/projects/tooling%2Fgnu-toolchains-for-arm/packages/generic/gnu-toolchain/15.3.rel1/arm-gnu-toolchain-15.3.rel1-mingw-w64-x86_64-arm-none-eabi.zip"
$armSha = "b85669d3408e2ae713b17b0cc59bc4ea26369a7f2bd19108fd11df7095f159e6"

function Need($cmd, $pkg) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        Write-Warning "$cmd not found, installing $pkg..."
        winget install --id $pkg -e --accept-source-agreements --accept-package-agreements
        if ($LASTEXITCODE -ne 0) { throw "winget failed to install $pkg (exit $LASTEXITCODE)" }
    }
}
Need cmake Kitware.CMake
Need ninja Ninja-build.Ninja
Need just Casey.Just
Need gh GitHub.cli

# winget writes PATH to the registry, not this session; append it so fresh installs are usable now
$env:PATH += ";" + [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")

function Fetch($url, $sha, $dest) {
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    $zip = Join-Path $tools "download.zip"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
    if ((Get-FileHash $zip -Algorithm SHA256).Hash -ne $sha) {
        Remove-Item $zip
        throw "sha256 mismatch for $url"
    }
    Expand-Archive -Path $zip -DestinationPath $dest -Force
    Remove-Item $zip
}

if (-not (Test-Path (Join-Path $picotoolDir "picotoolConfig.cmake"))) {
    Write-Host "downloading prebuilt picotool..."
    Fetch $ptUrl $ptSha $tools
}

if (-not (Test-Path (Join-Path $armDir "bin\arm-none-eabi-gcc.exe"))) {
    Write-Host "downloading arm gnu toolchain 15.3.rel1..."
    Fetch $armUrl $armSha $armDir
}

# the pico-sdk searches PICO_TOOLCHAIN_PATH before PATH, so an older arm-gcc on PATH can't win
[Environment]::SetEnvironmentVariable("PICO_TOOLCHAIN_PATH", $armDir, "User")
$env:PICO_TOOLCHAIN_PATH = $armDir

Write-Host "setup complete. build with: just elec build"
