{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    nixpkgs-esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
  };
  
  outputs = { self, nixpkgs, nixpkgs-esp-dev }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs { inherit system; overlays = [ (import "${nixpkgs-esp-dev}/overlay.nix") ]; };

      esp32-toolchain = pkgs.stdenv.mkDerivation {
        name = "riscv-esp32-firmware";
        src = ./.;
        nativeBuildInputs = with pkgs; [
          # commented for now
          # gcc-riscv32-esp32-elf-bin
          esp-idf
          gcc-xtensa-esp32-elf-bin
          openocd-esp32-bin

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

    in
    {

      packages.x86_64-linux.default = esp32-toolchain;

      devShells = {
        x86_64-linux = {
          default = pkgs.mkShell {
            shellHook = ''
              echo -e "use 'nix develop .#esp32'\nor 'nix develop .#fe310'."
              exit
            '';
          };
          # usage: nix develop .#mirrexagon
          mirrexagon = nixpkgs-esp-dev.devShells.x86_64-linux.esp32-idf {
            shellHook = ''
              export IDF_PATH=$(pwd)/esp-idf              
            '';
          };
          # usage: nix develop .#esp32
          esp32 = pkgs.mkShell {
            buildInputs = [
              esp32-toolchain
            ];
            shellHook = ''
              export IDF_PATH=$(pwd)/esp-idf
            '';
          };
        };
      };
    };
}
