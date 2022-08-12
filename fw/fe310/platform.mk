CC =     riscv32-none-elf-gcc
AR =     riscv32-none-elf-ar
RANLIB = riscv32-none-elf-ranlib

CFLAGS = -march=rv32g -mabi=ilp32d -mcmodel=medlow -ffunction-sections -fdata-sections --specs=nano.specs -O2
