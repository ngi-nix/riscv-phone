# TODO

- Revert all changes operated on the source files, namely `fw/fe310/Makefile` and `fw/fe310/platform.mk`.
- Open issue concerning `error: Unknown kernel: unknown`, when using the tupple `riscv32-unknown-elf`.
- Add upstream as an input and remove source files from repository.

## fw/fe310/test/Makfile
- Find out if we can do without `bash` at the beginning of line 29.
- Find out where is `riscv32-unknown-elf-gdb`, required by line 29 (`riscv32-none-elf` doesn't seem to provide it, nor `riscv32-none-elf-gdb` seems to exist). 
- Find out if we can getk rid of `-L../../../` at line 22.

## fw/fe310/platform.mk
- Find out why the the original arguments `-march=rv32imac -mabi=ilp32` at line 5 don't work (or whether the current working combination is ok), revert to original upstream preferably. 
- Revert to the original values upstream for `CC`, `AR`, and `RANLIB`.
