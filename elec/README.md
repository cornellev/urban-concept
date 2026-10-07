# elec

This folder holds the firmware for the car's RP2040 boards.

Each board has its own program in its own folder under `src/` (for example `template-project`).
`src/common/` holds code shared by all boards.

The Pico SDK is vendored at the repo root in `vendor/pico-sdk/` as a git submodule.

## Setup

The setup scripts install the general build tools, like CMake, Ninja, and Just. They do not
install the arm compiler or picotool. The first `just build` downloads the pinned versions (arm
compiler 15.3.rel1 and picotool 2.3.0) into `elec/.tools`. That download is 150 to 300 MB, takes
about 1 GB of disk, and needs internet once. Later builds reuse it. The build always
uses these copies, so another `arm-none-eabi-gcc` on your computer can't break it.

### Windows

- Make sure you have `winget` installed already. If not, install it
- Cd to the `elec/` folder:
  ```powershell
  cd C:\path\to\urban-concept\elec
  ```
- Run the setup script:
  ```powershell
  powershell -ExecutionPolicy Bypass -File setup/setup-windows.ps1
  ```
  It installs anything missing (CMake, Ninja, Just, and Python) with `winget`,
  and fetches the pico-sdk submodules. Windows may pop up permission windows
  during installs. Click yes for all of them.
- Close and open a new terminal, because PATH may not be updated yet.

### macOS

- Install [Homebrew](https://brew.sh) if you don't have it
- cd to the `elec/` folder:
  ```sh
  cd /path/to/urban-concept/elec
  ```
- Run the setup script from the `elec/` folder:
  ```sh
  ./setup/setup-macos.sh
  ```
  It installs CMake, Ninja, and Just with Homebrew, and fetches the pico-sdk submodules.

Intel Macs aren't supported, because Arm does not publish an Intel Mac build of this compiler.

### Linux

On Debian 12 or newer, Ubuntu 22.04 or newer, or Arch, on an x86_64 or ARM64 computer:

- cd to the `elec/` folder:
  ```sh
  cd /path/to/urban-concept/elec
  ```
- Run the setup script from the `elec/` folder:
  ```sh
  ./setup/setup-linux.sh
  ```
  It installs CMake, Ninja, Just, Python, and g++ with `apt` or `pacman`, and fetches the pico-sdk
  submodules. It also adds a udev rule so `just flash` works without root.

On other distros, install CMake 3.21 or newer, Ninja, Just 1.52.0 or newer, Python 3, and libusb
1.0 with your package manager.

### Nix

Use the provided flake. `nix develop` (or `direnv allow`) sets everything up. It sets
`ELEC_SYSTEM_TOOLS`, so the build uses the flake's arm compiler and picotool instead of downloading
its own.

## Building

From the `elec/` folder:

```sh
just build                         # build every project
just build-target template-project # build just one project
just clean                         # delete the build files and start fresh
just doctor                        # check if tools are installed and working
```

(From the repo root instead, add the `elec` prefix, ie `just elec build`.)

The project name is the folder name under `src/`. After a build, the file to flash onto the board is located at:

```
elec/build/src/<project>/<project>.uf2
```

If a build fails right after installing or switching tools (for example it uses
the wrong compiler), run `just clean` and build again. The old `build/` folder
caches the previous compiler, so a stale cache can break the next build.

## Flashing (putting firmware on a board)

### Via a command

```sh
just flash template-project
```

This builds the project and loads it onto the board. If the board was already running our firmware,
you don't need to put it into BOOTSEL as this command makes the board automatically reboot into
BOOTSEL first.

On Windows, this may need a USB driver for picotool.

If it says "no accessible RP2 devices", try manually putting the board into BOOTSEL mode (see
below) and running it again. If it still fails, use the manual method.

### Manually flashing

To put a board in BOOTSEL mode:

- Unplug it
- Hold the BOOTSEL button
- Plug it back in while still holding the button down

In BOOTSEL mode the board shows up as a USB drive called **`RPI-RP2`**. Just drag the
project's `.uf2` file (see the path above) onto that drive. The board flashes itself and restarts

## Adding a new board

Each subdirectory of `src/` is an independent project with its own
`CMakeLists.txt`. To add one

- Copy `src/template-project` to a new folder. The folder name becomes the target name
- Write the board in `main.cpp`
- In the board's `CMakeLists.txt`, list extra libraries in `add_board(...)` and any source files
  besides `main.cpp` in `target_sources(...)`
- Register the folder in the firmware inventory in `elec/CMakeLists.txt`
- Compile with `just build-target {project-name}`
- Flash with `just flash {project-name}`

For example, a board in `src/steering/` with `main.cpp`, `pid.cpp`, and `encoder.cpp` has this
`src/steering/CMakeLists.txt`:

```cmake
add_board(hardware_pwm)
target_sources(steering PRIVATE pid.cpp encoder.cpp)
```

## Editor

### VS Code

We use the clangd extension instead of Microsoft's C/C++ IntelliSense because it integrates better
with other platforms' tools. For setup:

- Build once first, because clangd reads `build/compile_commands.json`. For the chuds library's own
  tests and examples, also run `just cev-lib chuds build` from the repo root (macOS and Linux
  only, since it needs a C++ compiler for your computer)
- chuds' SocketCAN code shows errors on macOS and Windows, because it uses Linux-only headers.
  `telem/shm` shows errors anywhere but the Pi, because it needs the Pi's pigpio library
- Open the repo root (`urban-concept/`) in VS Code, not `elec/`
- Install the recommended clangd extension when VS Code asks
- If VS Code asks whether you trust the folder, click Trust
- If it says clangd was not found on your PATH, click Install
- Use clangd 19 or newer. Older ones show false errors about `std::expected`, and Ubuntu's `clangd`
  package is 18. If you see those errors, run "clangd: Check for language server update" from the
  command palette. On ARM Linux, which that can't download for, install `clangd-19` or newer with
  your package manager

### Neovim

Use clangd 19 or newer and Neovim 0.11 or newer. No plugin is needed. Build once first, as in the
VS Code steps. Then add this to your config. It passes the same `--query-driver` flag VS Code uses:

```lua
vim.lsp.config('clangd', {
  cmd = { 'clangd', '--query-driver=**/arm-none-eabi-*,**/clang++' },
  filetypes = { 'c', 'cpp' },
  root_markers = { '.clangd', '.git' },
})
vim.lsp.enable('clangd')
```

### Other editors

Pass `--query-driver=**/arm-none-eabi-*,**/clang++` to clangd.
