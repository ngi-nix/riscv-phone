CC =     riscv32-none-elf-gcc
AR =     riscv32-none-elf-ar
RANLIB = riscv32-none-elf-ranlib

CFLAGS = -march=rv32imac -mabi=ilp32 -mcmodel=medlow -ffunction-sections -fdata-sections --specs=nano.specs -O2
