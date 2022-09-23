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

      overlays.default = _: prev: rec {
        esp32 = pkgs.callPackage ./esp32.nix { inherit riscvphone-src nixpkgs-esp-dev; };
        fe310 = prev.callPackage ./fe310.nix { inherit riscvphone-src riscv-toolchain nanolibsPath; };
        nanolibsPath = prev.callPackage ./nanolibs.nix { inherit (riscv-toolchain) newlib-nano; };
      };

      packages.x86_64-linux = {
        inherit (self.overlays.default null nixpkgs.legacyPackages.${system}) esp32 nanolibsPath fe310;
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
            mkdir -p src && cp -r $src/* ./src && chmod -R 755 ./src

            # make will generate a full sdkconfig configuration, just override a few values.
            cp sdkconfig.defaults ./src/fw/esp32
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
            export NANOLIBS_PATH=${riscv-toolchain.newlib-nano}/riscv64-none-elf/lib/*.a
            export RISCV_HOME=${riscv-toolchain.buildPackages.gcc}
            export RISCV_OPENOCD_HOME=${pkgs.openocd}

            # execute the nanolibs script
            nix run .#nanolibsPath

            # copy upstream files and set permissions.
            mkdir -p src && cp -r $src/* ./src && chmod -R 755 ./src

            # replace the original tuple in the source file for one that we can find.
            sudo sed -i 's/riscv64-unknown-elf/riscv64-none-elf/g' ./src/fw/fe310/platform.mk
          '';
        };
        
      };
    };
}
