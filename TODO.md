# TODO

- ~~Revert all changes operated on the source files, namely `fw/fe310/Makefile` and `fw/fe310/platform.mk`.~~ &#x2714;
- ~~Open issue concerning `error: Unknown kernel: unknown`, when using the tupple `riscv32-unknown-elf`.~~ obsolete
- ~~Add upstream as an input and remove source files from repository.~~ &#x2714;

## ~~fw/fe310/test/Makfile~~
- ~~Find out if we can do without `bash` at the beginning of line 29.~~ obsolete
- ~~Find out where is `riscv32-unknown-elf-gdb`, required by line 29 (`riscv32-none-elf` doesn't seem to provide it, nor `riscv32-none-elf-gdb` seems to exist).~~ obsolete
- ~~Find out if we can getk rid of `-L../../../` at line 22.~~ obsolete

## fw/fe310/platform.mk
- ~~Find out why the the original arguments `-march=rv32imac -mabi=ilp32` at line 5 don't work (or whether the current working combination is ok), revert to original upstream preferably.~~ obsolete
- ~~Revert to the original values upstream for `CC`, `AR`, and `RANLIB`.~~ &#x2714;

All SOLVED with branch [flake-only](https://github.com/ngi-nix/riscv-phone/tree/flake-only), which adds the upstream repository as an input, ready to be consumed as an up-to-date dependency if based against the $HEAD of the upstream or pinned to a specific revision. It also removes the need for the source files to be added manually. This also means that we are now working against the original source files and the this TODO list ceases to make sense. 
