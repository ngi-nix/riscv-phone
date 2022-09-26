{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/";
    nixpkgs-esp32.url = "github:NixOS/nixpkgs/c11d08f02390aab49e7c22e6d0ea9b176394d961";
    # Requires specific nixpkgs version due to: https://github.com/mirrexagon/nixpkgs-esp-dev/issues/10
    nixpkgs-esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
    riscvphone-src = {
      url = "git://majstor.org/rvPhone.git";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, nixpkgs-esp-dev, riscvphone-src, nixpkgs-esp32 }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs-esp32 {
        inherit system;
        overlays = [
          (import "${nixpkgs-esp-dev}/overlay.nix")
        ];
      };

      riscv-toolchain = import nixpkgs {
        localSystem = system;
        crossSystem = {
          config = "riscv64-none-elf";
          libc = "newlib-nano";
          abi = "ilp64";
        };
      };

    in
    {

      overlays.default = _: prev: {
        esp32 = pkgs.callPackage ./esp32.nix { inherit riscvphone-src nixpkgs-esp-dev; };
        fe310 = prev.callPackage ./fe310.nix { inherit riscvphone-src riscv-toolchain; };
      };

      packages.x86_64-linux = {
        inherit (self.overlays.default null nixpkgs.legacyPackages.${system}) esp32 fe310;
      };

      devShells.x86_64-linux = {
        default = pkgs.mkShell {
          shellHook = ''
            echo -e "use 'nix develop .#esp32'\nor 'nix develop .#fe310'."
            exit
          '';
        };

        # usage: nix develop .#esp32
        esp32 = pkgs.mkShell {
          src = "${riscvphone-src}";
          buildInputs = with pkgs; [
            nixpkgs-esp-dev.packages.x86_64-linux.esp-idf
            gcc-xtensa-esp32-elf-bin
          ];
          shellHook = ''
            # copy upstream files and set permissions.
            mkdir -p esp32Shell && cp -r $src/* ./esp32Shell && chmod -R 755 ./esp32Shell

            # generate an empty sdkconfig file, which overrides the menu prompt.
            touch ./esp32Shell/fw/esp32/sdkconfig

            # override certain values when populating sdkconfig with make.
            cp ./sdkconfig.defaults ./esp32Shell/fw/esp32

            # make commands: http://majstor.org/rvphone/build.html
            cd ./esp32Shell/fw/esp32
          '';
        };

        # usage: nix develop .#fe310
        fe310 = pkgs.mkShell {
          src = "${riscvphone-src}";
          buildInputs = with pkgs; [
            riscv-toolchain.buildPackages.gcc
            openocd
          ];
          shellHook = ''
            export RISCV_HOME=${riscv-toolchain.buildPackages.gcc}
            export RISCV_OPENOCD_HOME=${pkgs.openocd}

            # copy upstream files and set permissions.
            mkdir -p fe310Shell && cp -r $src/* ./fe310Shell && chmod -R 755 ./fe310Shell

            # replace the original tuple in the source file for one that we can find.
            sudo sed -i 's/riscv64-unknown-elf/riscv64-none-elf/g' ./fe310Shell/fw/fe310/platform.mk

            # make commands: http://majstor.org/rvphone/build.html
            cd ./fe310Shell/fw/fe310
          '';
        };

      };
    };
}