{
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs";

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        riscv-toolchain =
          import nixpkgs {
            localSystem = "${system}";
            crossSystem = {
              config = "riscv32-none-elf";
              libc = "newlib-nano";
              abi = "ilp32";
            };
          };

      in {

        packages = {

          nanolibsPath = pkgs.writeShellApplication {
            name = "nanolibs-path";
            runtimeInputs = with pkgs; [
              riscv-toolchain.newlib-nano
            ];
            text = builtins.readFile ./nanolibs-script.sh;
          };

          fe310 = riscv-toolchain.stdenv.mkDerivation {
            name = "riscv-fe310-firmware";
            src = ./.;
            buildInputs = with pkgs; [
              riscv-toolchain.buildPackages.gcc
              openocd
            ];
            preBuild = ''
              export NANOLIBS_PATH=${riscv-toolchain.newlib-nano}/riscv32-none-elf/lib/*.a
              nix run .#nanolibsPath
            '';
            buildPhase = ''
              make -C fw/fe310
            '';
            installPhase = ''
              cp -r fw/fe310/libeos.a $out
            '';
            checkPhase = ''
              make -C fw/fe310/test
            '';
          };
        };

        devShells = {

          # usage: nix develop .#fe310Shell
          fe310Shell = pkgs.mkShell {
            buildInputs = with pkgs; [
              riscv-toolchain.buildPackages.gcc
              openocd
            ];
            shellHook = ''
              export NANOLIBS_PATH=${riscv-toolchain.newlib-nano}/riscv32-none-elf/lib/*.a
              nix run .#nanolibsPath
            '';
          };
        };
      });
}
