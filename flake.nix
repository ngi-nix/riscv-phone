{
  inputs.flake-utils.url = "github:numtide/flake-utils";
  #inputs.nixpkgs.url = "github:NixOS/nixpkgs";

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        riscv-toolchain =
#          (import <nixpkgs> {}).pkgsCross.riscv32;
#          (import <nixpkgs> {}).pkgsCross.riscv64;
          import nixpkgs {
            localSystem = "${system}";
            crossSystem = {
              config = "riscv64-none-elf";
              libc = "newlib";
              abi = "ilp32";
            };
          };
      in
        {
          packages.fe310 = riscv-toolchain.stdenv.mkDerivation {
            name = "riscv-fe310-firmware";

            src = ./.;

            buildInputs = with pkgs; [
              riscv-toolchain.buildPackages.gcc
              riscv-toolchain.buildPackages.binutils.bintools
            ];

            buildPhase = ''
              make -C fw/fe310
            '';

            installPhase = ''
              cp -r fw/fe310/libeos.a $out
            '';

            # TODO add check phase once we can build tests
            # checkPhase = ''
            #   make -C fw/fe310/test
            # '';

          };

          devShells = {
            # usage: nix develop .#riscvShell
            riscvShell = pkgs.mkShell {
              buildInputs = with pkgs; [
                riscv-toolchain.buildPackages.gcc
                riscv-toolchain.buildPackages.binutils.bintools
                openocd
                mkspiffs-presets.esp-idf
              ];
              shellHook = ''
                RISCV_HOME=${riscv-toolchain.buildPackages.gcc}
                RISCV_OPENOCD_PATH=${pkgs.openocd}
              '';
            };
            # usage: nix develop .#esp32Shell
            esp32Shell = pkgs.mkShell {
              buildInputs = with pkgs; [
                # TODO get working
                mkspiffs-presets.esp-idf
              ];
            };
          };
        }
    );

}
