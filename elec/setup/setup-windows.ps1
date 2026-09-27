# set up the windows build tools for rp2040 firmware
# installs any missing tools via winget
# the first build downloads the pinned arm compiler and picotool into elec/.tools
# after this, build from elec/ with: just build

$ErrorActionPreference = "Stop"

function Need($cmd, $pkg) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        Write-Warning "$cmd not found, installing $pkg..."
        winget install --id $pkg -e --accept-source-agreements --accept-package-agreements
        if ($LASTEXITCODE -ne 0) { throw "winget failed to install $pkg (exit $LASTEXITCODE)" }
    }
}

# winget writes PATH to the registry, not open shells, so a rerun would miss tools an earlier run installed
$env:PATH += ";" + [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")

Need cmake Kitware.CMake
Need ninja Ninja-build.Ninja
Need just Casey.Just
# pico-sdk's boot_stage2 needs python
# check for the py launcher because python.exe may be the microsoft store stub
Need py Python.Python.3.14

# fetch the pico-sdk and the one nested submodule the build needs
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
git -C $repo submodule update --init
if ($LASTEXITCODE -ne 0) { throw "git submodule update failed (exit $LASTEXITCODE)" }
git -C (Join-Path $repo "vendor/pico-sdk") submodule update --init lib/tinyusb
if ($LASTEXITCODE -ne 0) { throw "git submodule update failed (exit $LASTEXITCODE)" }

Write-Host "setup complete. build from elec/ with: just build"
