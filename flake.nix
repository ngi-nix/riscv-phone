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
        src = riscvphone-src;
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
        preBuild = ''
          export CONFIG_LWIP_PPP_SUPPORT=y
          export CONFIG_LWIP_PPP_NOTIFY_PHASE_SUPPORT=y
          export CONFIG_LWIP_PPP_PAP_SUPPORT=y
          export CONFIG_LWIP_PPP_CHAP_SUPPORT=y

          export CONFIG_UNITY_ENABLE_IDF_TEST_RUNNER=false
        '';
        buildInputs = [

        ] ++ esp32-toolchain.buildInputs;

        nativeBuildInputs = [

        ] ++ esp32-toolchain.nativeBuildInputs;

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
          mirrexagon = nixpkgs-esp-dev.devShells.x86_64-linux.esp32-idf //
            {
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
