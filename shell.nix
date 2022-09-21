{ pkgs ? import <nixpkgs> { } }:

let
  #  crossPkgs = (import <nixpkgs> {}).pkgsCross.riscv32-embedded;
  #  crossPkgs = (import <nixpkgs> {}).pkgsCross.riscv64;
  crossPkgs =
    import <nixpkgs> {
      # uses GCC and newlib
      crossSystem = {
        #config = "riscv64-none-elf";
        #config = "riscv32-none-elf";
        config = "riscv32-unknown-none-elf";
        #config = "riscv32-unknown-linux-gnu";
        abi = "ilp32";
      };
    };
in

with import <nixpkgs> { };
{
  devShell = pkgs.mkShell {
    buildInputs = with pkgs; [
      openocd
      mkspiffs-presets.esp-idf
    ];
    shellHook = ''
      export RISCV_HOME=${crossPkgs.buildPackages.gcc}
      export RISCV_OPENOCD_PATH=${pkgs.openocd}
    '';
  };
}
