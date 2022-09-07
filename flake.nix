{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/c11d08f02390aab49e7c22e6d0ea9b176394d961";
  inputs.nixpkgs-esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
  

  outputs = { self, nixpkgs, nixpkgs-esp-dev }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs { inherit system; overlays = [ (import "${nixpkgs-esp-dev}/overlay.nix") ]; };

      esp32-toolchain-drv = pkgs.stdenv.mkDerivation {
        name = "riscv-esp32-firmware";
        src = ./.;
        nativeBuildInputs = with pkgs; [
          gcc-xtensa-esp32-elf-bin
          openocd-esp32-bin
          esp-idf
        ];
        buildInputs = with pkgs; [
          esptool
          git
          wget
          gnumake
          flex
          bison
          gperf
          pkgconfig
          cmake
          ninja
          ncurses5
        ];
        preBuild = ''
          export IDF_PATH=$(pwd)/esp-idf
        '';

      };

      esp32-toolchain = pkgs.callPackage esp32-toolchain-drv { };

    in
    {

      packages.x86_64-linux.default = esp32-toolchain;

      devShells = {
        x86_64-linux.default = pkgs.mkShell {
          shellHook = ''            
          echo use 
          echo "nix develop .#esp32Shell"
          echo or 
          echo "nix develop .#fe310Shell"
          exit
          '';
        };
        # usage: nix develop .#esp32Shell
        x86_64-linux.esp32Shell = pkgs.mkShell {
          buildInputs = [
            esp32-toolchain
          ];
          shellHook = ''            
            export IDF_PATH=$(pwd)/esp-idf
          '';
        };
      };
    };
}
