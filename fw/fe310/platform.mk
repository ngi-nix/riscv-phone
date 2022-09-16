CC =     riscv32-none-elf-gcc
#        ^^^^^^^^^^^^^^^^^^^^ revert to upstream value, if possible
AR =     riscv32-none-elf-ar
#        ^^^^^^^^^^^^^^^^^^^ revert to upstream value, if possible
RANLIB = riscv32-none-elf-ranlib
#        ^^^^^^^^^^^^^^^^^^^^^^^ revert to upstream value, if possible
CFLAGS = -march=rv32g -mabi=ilp32d -mcmodel=medlow -ffunction-sections -fdata-sections --specs=nano.specs -O2
#        ^^^^^^^^^^^^^^^^^^^^^^^^^ working combination, in the original: -march=rv32imac -mabi=ilp32 
