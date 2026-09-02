{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
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
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      imports = [ treefmt-nix.flakeModule ];

      perSystem =
        { pkgs, config, ... }:
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
              gh
            ];

            # pico-sdk is vendored at elec/pico-sdk
            CMAKE_PREFIX_PATH = "${pkgs.picotool}";
            CMAKE_EXPORT_COMPILE_COMMANDS = "1";
            CMAKE_GENERATOR = "Ninja";
          };

          treefmt = {
            projectRootFile = "flake.nix";
            settings.global.excludes = [
              "elec/pico-sdk/**"
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
