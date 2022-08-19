{
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs";

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        nanolibs-script = {       
          name = "nanolibs-path";
          script = pkgs.writeShellScriptBin nanolibs-script.name ''
            rm -fr nanolibs/*.a
            mkdir -p nanolibs
            for file in ${riscv-toolchain.newlib-nano}/riscv32-none-elf/lib/*.a; do
               ln -s $file nanolibs
            done
            for file in nanolibs/*.a; do
               mv "$file" "''${file%%.a}_nano.a"                
            done
          '';
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
      in
        {
          packages = {

            fe310 = riscv-toolchain.stdenv.mkDerivation {
              name = "riscv-fe310-firmware";

              src = ./.;

              buildInputs = with pkgs; [
                riscv-toolchain.buildPackages.gcc
                openocd
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
            
          nanolibsPath = pkgs.symlinkJoin {
            name = "nanolibs-path";
            paths = [ nanolibs-script.script ];
            buildInputs = with pkgs; [
              makeWrapper
            ];
            postBuild = ''
              wrapProgram $out/bin/${nanolibs-script.name}
            '';
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
