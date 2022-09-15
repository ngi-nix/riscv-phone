{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs";
  
  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs { inherit system; };
        
      riscv-toolchain = import nixpkgs {
        localSystem = system;
        crossSystem = {
          config = "riscv32-none-elf";
          libc = "newlib-nano";
          abi = "ilp32";
        };          
      };
      
      nanolibsPath = pkgs.writeShellApplication {
        name = "nanolibs-path";
        runtimeInputs = with nixpkgs; [
          riscv-toolchain.newlib-nano
        ];
        text = builtins.readFile ./nanolibs-script.sh;
      };
      
      fe310-toolchain = riscv-toolchain.stdenv.mkDerivation {
        name = "riscv-fe310-firmware";
        src = ./.;
        buildInputs = with pkgs; [
          riscv-toolchain.buildPackages.gcc          
        ];              
        preBuild = ''
          export NANOLIBS_PATH=${riscv-toolchain.newlib-nano}/riscv32-none-elf/lib/*.a
        BUILD_DIR = (placeholder "out") + "/build";
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

    in
      {
        packages = {
          x86_64-linux = {
            nanolibsPath = nanolibsPath;
            # usage: nix build .#fe310
            fe310 = fe310-toolchain;
          };
        };
      
        devShells = {
          # usage: nix develop .#fe310
          x86_64-linux.fe310 = pkgs.mkShell {
            buildInputs = with pkgs; [
              riscv-toolchain.buildPackages.gcc          
            ];
          shellHook = ''
            export NANOLIBS_PATH=${riscv-toolchain.newlib-nano}/riscv32-none-elf/lib/*.a
            nix run .#nanolibsPath
          '';
          };
        };
        
      };
}
