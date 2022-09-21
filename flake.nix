{
  inputs = {
    # nixpkgs.url = "github:NixOS/nixpkgs";
    nixpkgs.url = "github:NixOS/nixpkgs/c11d08f02390aab49e7c22e6d0ea9b176394d961";
    nixpkgs-esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
    riscvphone-src = {
      url = "git://majstor.org/rvPhone.git";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, nixpkgs-esp-dev, riscvphone-src }:
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

      packages.x86_64-linux.esp32 = pkgs.stdenv.mkDerivation {
        name = "esp32";
        src = riscvphone-src;
        dontUseCmakeConfigure = true;
        dontUseNinjaBuild = true;
        dontUseNinjaInstall = true;
        dontUseNinjaCheck = true;
        configurePhase = ''
          cd fw/esp32
          export PROJECT_VER=${riscvphone-src.rev}
          export LC_ALL=C
          export LC_CTYPE="C.UTF-8"
          cat ${fw/esp32/sdkconfig} > sdkconfig
        '';
        buildPhase = ''
          make
        '';
        installPhase = ''
        mkdir -p $out/bin
        cp -r build/* $out/bin
        '';
        nativeBuildInputs = [
          nixpkgs-esp-dev.packages.x86_64-linux.esp-idf
          nixpkgs-esp-dev.packages.x86_64-linux.gcc-xtensa-esp32-elf-bin
          nixpkgs-esp-dev.packages.x86_64-linux.openocd-esp32-bin
          pkgs.esptool
        ] ++ esp32-toolchain.nativeBuildInputs;
        buildInputs = [
          nixpkgs-esp-dev.packages.x86_64-linux.esp-idf
          pkgs.git
          nixpkgs-esp-dev.packages.x86_64-linux.gcc-xtensa-esp32-elf-bin
          nixpkgs-esp-dev.packages.x86_64-linux.openocd-esp32-bin
          pkgs.esptool
        ] ++ esp32-toolchain.buildInputs;


      };

      devShells = {
        x86_64-linux = {
          default = pkgs.mkShell {
            shellHook = ''
              echo -e "use 'nix develop .#esp32'\nor 'nix develop .#fe310'."
              exit
            '';
          };
          # usage: nix develop .#mirrexagon
          mirrexagon = nixpkgs-esp-dev.devShells.x86_64-linux.esp32-idf;
          # //
          # {
          #   shellHook = ''
          #     export IDF_PATH=$(pwd)/esp-idf              
          #   '';
          # };
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
