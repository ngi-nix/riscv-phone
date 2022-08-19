#!/usr/bin/env bash

rm -fr nanolibs/*.a
mkdir -p nanolibs
for file in "${riscv-toolchain.newlib-nano}"/riscv32-none-elf/lib/*.a; do
   ln -s "$file" nanolibs
done
for file in nanolibs/*.a; do
   mv "$file" "${file%%.a}_nano.a"                
done
