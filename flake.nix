{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs";
  inputs.riscvphone-src = {
    url = "git://majstor.org/rvPhone.git";
    flake = false;
  };
  
  outputs = { self, nixpkgs, riscvphone-src }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs { inherit system; inherit riscvphone-src; };
        
      riscv-toolchain = import nixpkgs {
        localSystem = system;
        crossSystem = {
          config = "riscv64-none-elf";
          libc = "newlib-nano";
          abi = "ilp64";
        };          
      };
      
      nanolibsPath = pkgs.writeShellApplication {
        name = "nanolibs-path";
        runtimeInputs = with nixpkgs; [
          riscv-toolchain.newlib-nano
        ];
        text = builtins.readFile ./nanolibs-script.sh;
      };
      
      fe310-drv = riscv-toolchain.stdenv.mkDerivation {
        name = "riscv-fe310-firmware";
        src = "${riscvphone-src}";
        buildInputs = with pkgs; [
          riscv-toolchain.buildPackages.gcc
          nanolibsPath
          openocd
        ];
        configurePhase = ''
          # create the src and build directories.
          mkdir -p $out/build && mkdir -p $out/src

          # copy the upstream files and set permissions to make changes.
          cp -r $src/* $out/src && chmod -R 755 $out
        '';
        buildPhase = ''
          export NANOLIBS_PATH=${riscv-toolchain.newlib-nano}/riscv64-none-elf/lib/*.a
          export RISCV_HOME=${riscv-toolchain.buildPackages.gcc}
          export RISCV_OPENOCD_HOME=${pkgs.openocd}

          # execute the nanolibs script.
          ${nanolibsPath}/bin/nanolibs-path

          # replace the original tuple in the source file for one that we can find.
          sed -i 's/riscv64-unknown-elf/riscv64-none-elf/g' $out/src/fw/fe310/platform.mk

          make -C $out/src/fw/fe310
        '';
        installPhase = ''
          # move compiled object and archive files into the build directory.
          find $out/src/fw/fe310 -type f \( -iname "*.o" -o -iname "libe*.a" \) -exec mv {} $out/build \;
        '';
      };

    in
      {
        packages = {
          x86_64-linux = {
            nanolibsPath = nanolibsPath;
            # usage: nix build .#fe310
            fe310 = fe310-drv;
          };
        };
      
        devShells = {
          # usage: nix develop .#fe310
          # compile: cd src/fw/fe310 --> make
          # clean up: cd src/fw/fe310 --> make clean
          x86_64-linux.fe310 = pkgs.mkShell {
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

              # copy the upstream files and set permissions to make changes.
              mkdir -p src && cp -r $src/* ./src && chmod -R 755 ./src

              # replace the original tuple in the source file for one that we can find.
              sudo sed -i 's/riscv64-unknown-elf/riscv64-none-elf/g' ./src/fw/fe310/platform.mk
            '';
          };
        };
        
      };
}
