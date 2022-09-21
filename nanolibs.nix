{ writeShellScriptBin
, newlib-nano
}:
writeShellScriptBin "nanolibs-path"
  ''
    rm -fr nanolibs/*.a
    mkdir -p nanolibs
    for file in ${newlib-nano}/riscv64-none-elf/lib/*.a; do
       ln -s "$file" nanolibs
    done
    for file in nanolibs/*.a; do
       mv "$file" "''${file%%.a}_nano.a"
    done''

