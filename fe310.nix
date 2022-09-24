{ riscv-toolchain,
  openocd,
  riscvphone-src,
}:

riscv-toolchain.stdenv.mkDerivation {
  name = "riscv-fe310-firmware";
  src = "${riscvphone-src}";
  dontInstall = true;

  buildInputs = [
    riscv-toolchain.buildPackages.gcc
    openocd
  ];

  configurePhase = ''
    export RISCV_HOME=${riscv-toolchain.buildPackages.gcc}
    export RISCV_OPENOCD_HOME=${openocd}

    mkdir -p $out && cp -r . $out  && chmod -R 755 $out

    # replace the original tuple in the source file for one that we can find.
    sed -i 's/riscv64-unknown-elf/riscv64-none-elf/g' $out/fw/fe310/platform.mk

    cd $out/fw/fe310
  '';

  installPhase = ''
    #make upload
  '';
}
