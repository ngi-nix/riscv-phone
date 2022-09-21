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

      pkgs = import nixpkgs {
        inherit system;
        overlays = [
          (import "${nixpkgs-esp-dev}/overlay.nix")
          self.overlays.default
        ];
      };
    in
    {

      overlays.default = final: prev: {
        esp32-toolchain = prev.stdenv.mkDerivation {
          name = "riscv-esp32-firmware";
          src = ./.;

          nativeBuildInputs = with prev; [
            # commented for now
            # gcc-riscv32-esp32-elf-bin
            esp-idf
            gcc-xtensa-esp32-elf-bin
            openocd-esp32-bin

          ];

          buildInputs = with prev; [
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


        esp32 = prev.stdenv.mkDerivation {
          name = "esp32";
          src = riscvphone-src;
          dontUseCmakeConfigure = true;
          dontUseNinjaBuild = true;
          dontUseNinjaInstall = true;
          dontUseNinjaCheck = true;

          PROJECT_VER = riscvphone-src.rev;
          LC_ALL = "C";
          LC_CTYPE = "C.UTF-8";

          configurePhase = ''
            cd fw/esp32
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
            prev.esptool
          ] ++ final.esp32-toolchain.nativeBuildInputs;

          buildInputs = [
            nixpkgs-esp-dev.packages.x86_64-linux.esp-idf
            prev.git
            nixpkgs-esp-dev.packages.x86_64-linux.gcc-xtensa-esp32-elf-bin
            nixpkgs-esp-dev.packages.x86_64-linux.openocd-esp32-bin
            prev.esptool
          ] ++ final.esp32-toolchain.buildInputs;
        };
      };

      packages.x86_64-linux = {
        inherit (pkgs) esp32 esp32-toolchain;
        default = pkgs.esp32-toolchain;
      };

      devShells.x86_64-linux = {
        default = pkgs.mkShell {
          shellHook = ''
            echo -e "use 'nix develop .#esp32'\nor 'nix develop .#fe310'."
            exit
          '';
        };
        # usage: nix develop .#mirrexagon
        mirrexagon = nixpkgs-esp-dev.devShells.x86_64-linux.esp32-idf;
        # usage: nix develop .#esp32
        esp32 = pkgs.mkShell {
          buildInputs = [
            pkgs.esp32-toolchain
          ];
          shellHook = ''
            export IDF_PATH=$(pwd)/esp-idf
          '';
        };
      };
    };
}
