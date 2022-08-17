{
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs";

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        nanolibsPatch = ''
          rm -fr nanolibs/*.a
          mkdir -p nanolibs
          for file in ${riscv-toolchain.newlib-nano}/riscv32-none-elf/lib/*.a; do
             ln -s $file nanolibs
          done
          for file in nanolibs/*.a; do
             mv "$file" "''${file%%.a}_nano.a"
          done
        '';

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
              
              shellHook = nanolibsPatch;

            };
            
          };
        }
    );

}
