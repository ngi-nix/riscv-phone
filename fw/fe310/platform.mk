CC =     riscv64-none-elf-gcc
AR =     riscv64-none-elf-ar
RANLIB = riscv64-none-elf-ranlib

CFLAGS = -march=rv32imac -mabi=ilp32 -mcmodel=medlow -ffunction-sections -fdata-sections -O2
