{
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs";

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        nanolibs-script = rec {
          name = "nanolibs-path";
          source = builtins.readFile ./nanolibs-script.sh;
          script = (pkgs.writeShellScriptBin name source).overrideAttrs(old: {
            buildCommand = "${old.buildCommand}\n patchShebangs $out";
          });
          buildInputs = [
            riscv-toolchain.newlib-nano
          ];
        };

        riscv-toolchain =
          import nixpkgs {
            localSystem = "${system}";
            crossSystem = {
              config = "riscv32-none-elf";
              libc = "newlib-nano";
              abi = "ilp32d";
            };
          };

      in {

        packages = {

#          nanolibsPath = pkgs.writeShellApplication {
#            name = "nanolibs-path";
#            runtimeInputs = with pkgs; [
#              riscv-toolchain.newlib-nano
#            ];
#            text = builtins.readFile ./nanolibs-script.sh;
#          };

          nanolibsPath = pkgs.symlinkJoin {
            name = "nanolibs-path";
            paths = [ nanolibs-script.script ] ++ nanolibs-script.buildInputs;
            nativeBuildInputs = with pkgs; [
              makeWrapper
            ];
            postBuild = ''
              wrapProgram $out/bin/${nanolibs-script.name}  --prefix PATH : $out/bin
            '';
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
