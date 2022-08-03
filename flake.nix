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
#        with import <nixpkgs> {};
        {
          devShell = pkgs.mkShell {
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
        }
    );
 
}
