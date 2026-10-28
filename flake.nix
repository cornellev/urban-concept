{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    # only for zenoh: 1.10.0 in nixpkgs above points cmake at libzenohc in its dev output
    # drop it once nixpkgs above has zenoh-c 1.10.1, which keeps the firmware toolchain pinned
    nixpkgs-zenoh.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    inputs@{ flake-parts, treefmt-nix, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];

      imports = [ treefmt-nix.flakeModule ];

      perSystem =
        {
          pkgs,
          config,
          inputs',
          ...
        }:
        {
          devShells.default = pkgs.mkShell {
            packages = with pkgs; [
              config.treefmt.build.wrapper
              gcc-arm-embedded
              picotool
              cmake
              cmake-language-server
              ninja
              just
              clang-tools
              # only run-clang-tidy, the rest of the unwrapped clang would shadow the compiler
              (writeShellScriptBin "run-clang-tidy" ''exec ${llvmPackages.clang-unwrapped}/bin/run-clang-tidy "$@"'')
              gh
              # auto/chuds_zenoh
              inputs'.nixpkgs-zenoh.legacyPackages.zenoh-c
              inputs'.nixpkgs-zenoh.legacyPackages.zenoh-cpp
            ];

            # pico-sdk is vendored at vendor/pico-sdk
            CMAKE_PREFIX_PATH = "${pkgs.picotool}";
            # use these tools instead of the build's own download of the pinned arm-gcc and picotool
            ELEC_SYSTEM_TOOLS = "1";
            PICO_TOOLCHAIN_PATH = "${pkgs.gcc-arm-embedded}";
            CMAKE_EXPORT_COMPILE_COMMANDS = "1";
            CMAKE_GENERATOR = "Ninja";
          };

          treefmt = {
            projectRootFile = "flake.nix";
            settings.global.excludes = [
              "vendor/pico-sdk/**"
              "**/build/**"
            ];
            programs = {
              clang-format.enable = true;
              nixfmt.enable = true;
              just.enable = true;
            };
          };

          formatter = config.treefmt.build.wrapper;
        };
    };
}
