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
              abi = "ilp32d";
            };
          };
      in
        {
          packages = {

            fe310 = riscv-toolchain.stdenv.mkDerivation {
              name = "riscv-fe310-firmware";

              src = ./.;

              buildInputs = with pkgs; [
                riscv-toolchain.buildPackages.gcc
              ];

              buildPhase = ''
                make -C fw/fe310
              '';

              installPhase = ''
                cp -r fw/fe310/libeos.a $out
              '';

              postInstall = ''
                mkdir -p $out
                for file in ${riscv-toolchain.newlib-nano}/riscv32-none-elf/lib/*.a; do
                  ln -s $file $out
                done
                for file in $out/*.a; do
                  mv "$file" "''${file%%.a}_nano.a"                
                done
              #'';

              checkPhase = ''
                make -C fw/fe310/test
              '';              
            };
            
            esp32 = riscv-toolchain.stdenv.mkDerivation {
              name = "riscv-esp32Shell-firmware";            
              src = ./.;
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
                rm -fr $out/*.a
                mkdir -p $out
                for file in ${riscv-toolchain.newlib-nano}/riscv32-none-elf/lib/*.a; do
                ln -s $file $out
                done
                for file in $out/*.a; do
                mv "$file" "''${file%%.a}_nano.a"                
                done
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
