{ riscv-toolchain,          
  nanolibsPath,
  openocd,
  riscvphone-src,
}:

riscv-toolchain.stdenv.mkDerivation {
  name = "riscv-fe310-firmware";
  src = "${riscvphone-src}";
        buildInputs = [
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
          export RISCV_OPENOCD_HOME=${openocd}

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
      }
