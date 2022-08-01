{ pkgs ? import <nixpkgs> {} }:

let
  crossPkgs =
    import <nixpkgs> {
      # uses GCC and newlib
      crossSystem = {
        config = "riscv64-none-elf";
        #system = "riscv64-unknown-none-elf";        
      };
    };
in

with import <nixpkgs> {};

#crossPkgs.mkShell {
#pkgsCross.riscv64.mkShell {
pkgsCross.riscv64-embedded.mkShell {
  
  buildInputs = with pkgs; [    
    openocd
    mkspiffs-presets.esp-idf    
  ];
 
  shellHook = ''
    RISCV_PATH=${pkgsCross.riscv64-embedded.gcc}
    RISCV_OPENOCD_PATH=${pkgs.openocd}
  '';

}
