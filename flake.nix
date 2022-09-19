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
        src = riscvphone-src + "/fw/esp32";
        dontUseCmakeConfigure = true;
        dontUseNinjaBuild = true;
        dontUseNinjaInstall = true;
        dontUseNinjaCheck = true;
        configurePhase = ''
          export PROJECT_VER=${riscvphone-src.rev}
          export LC_ALL=C
          export LC_CTYPE="C.UTF-8"
          cat ${fw/esp32/sdkconfig} > sdkconfig
          # sleep 99999999
        '';
        preBuild = ''
          # cat ${fw/esp32/sdkconfig} > sdkonfig
          # export HOME=$TMP
          # sleep 99999999
          # export CONFIG_LWIP_PPP_SUPPORT=y
          # export CONFIG_LWIP_PPP_NOTIFY_PHASE_SUPPORT=y
          # export CONFIG_LWIP_PPP_PAP_SUPPORT=y
          # export CONFIG_LWIP_PPP_CHAP_SUPPORT=y
          # export CONFIG_SDK_TOOLPREFIX="xtensa-esp32-elf-"
          #
          # export LC_ALL=C
          # export LC_CTYPE="C.UTF-8"

          # export CONFIG_UNITY_ENABLE_IDF_TEST_RUNNER=false
          # export IDF_PATH=./esp-idf              
          # idf.py set-target esp32
          # idf.py menuconfig
          # export LOCALE_ARCHIVE="$(nix-env --installed --no-name --out-path --query glibc-locales)/lib/locale/locale-archive"
          # export IDF_PATH=${riscvphone-src}/esp-idf
          # idf.py set-target esp32
          # idf.py menuconfig
        '';
        buildPhase = ''
          make
        '';
        nativeBuildInputs = [
          nixpkgs-esp-dev.packages.x86_64-linux.esp-idf
          nixpkgs-esp-dev.packages.x86_64-linux.gcc-xtensa-esp32-elf-bin
          nixpkgs-esp-dev.packages.x86_64-linux.openocd-esp32-bin
          nixpkgs-esp-dev.packages.x86_64-linux.esptool
        ] ++ esp32-toolchain.nativeBuildInputs;
        buildInputs = [
          nixpkgs-esp-dev.packages.x86_64-linux.esp-idf
          pkgs.git
          nixpkgs-esp-dev.packages.x86_64-linux.gcc-xtensa-esp32-elf-bin
          nixpkgs-esp-dev.packages.x86_64-linux.openocd-esp32-bin
          nixpkgs-esp-dev.packages.x86_64-linux.esptool
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
