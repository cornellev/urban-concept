# elec

This folder holds the firmware for the car's RP2040 boards.

"Firmware" is just the program that runs on a board. Each board has its own
program in its own folder under `src/` (for example `template-project`). The
Pico SDK is included here in `pico-sdk/`, so you don't install it separately.

each board has its own folder under `src/`, ie `template-project`.

The Pico SDK is under `pick-sdk/`, so it doesn't need to be separately installed.

## Setup

### Windows

- Make sure you have `winget` installed already. if not, install it
- cd to the `elec/` folder:
  ```powershell
  cd C:\path\to\urban-concept\elec
  ```
- Run the setup script:
  ```powershell
  powershell -ExecutionPolicy Bypass -File setup/setup-windows.ps1
  ```
  It installs anything missing (CMake, Ninja, Just, the GitHub CLI, and the arm compiler) and downloads picotool. Windows may pop up permission windows during installs. click yes for all of them.
- Close and open a new terminal, bc PATH may not be updated yet.

### macOS

1. Install [Homebrew](https://brew.sh) if you don't have it
2. Install the tools:
   ```sh
   brew install cmake ninja just gh picotool arm-none-eabi-gcc
   ```

### Nix

Use the provided flake. `nix develop` (or `direnv allow`) sets everything up.

## Building

From the `elec/` folder:

```sh
just build-all                 # build every project
just build template-project    # build just one project
just clean                     # delete the build files and start fresh
```

(From the repo root instead, add the `elec` prefix, ie `just elec build-all`.)

The project name is the folder name under `src/`. After a build, the file to flash onto the board is located at:

```
elec/build/src/<project>/<project>.uf2
```

## Flashing (putting firmware on a board)

First, put the board in BOOTSEL mode:

- unplug it
- hold the BOOTSEL button
- plug it back in while still holding the button down

### The simple way (works on every computer)

In BOOTSEL mode the board shows up as a USB drive called **`RPI-RP2`**. Just drag the project's `.uf2` file (see the path above) onto that drive. The board flashes itself and restarts

### The one-command way

```sh
just flash template-project
```

This builds the project and loads it onto the board. On macOS and nix this
works out of the box. On Windows it needs a USB driver for picotool; if it says
"no accessible RP2 devices," use the simple drag-and-drop way above instead.

## Adding a new board

Each subdirectory of `src/` is an independent project with its own
`CMakeLists.txt`. To add one

- copy `src/template-project`
- rename the target in its `CMakeLists.txt`
- add your source files
- link the libraries you need.
- compile with `just build {project-name}`
- flash with `just flash {project-name}`
