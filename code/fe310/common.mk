CC =     $(RISCV_HOME)/bin/riscv64-unknown-elf-gcc
AR =     $(RISCV_HOME)/bin/riscv64-unknown-elf-ar
RANLIB = $(RISCV_HOME)/bin/riscv64-unknown-elf-ranlib

CFLAGS = -march=rv32imac -mabi=ilp32 -mcmodel=medlow -ffunction-sections -fdata-sections --specs=nano.specs -O3
LDFLAGS = $(CFLAGS) -L$(FE310_HOME) -Wl,--gc-sections -nostartfiles -nostdlib -Wl,--start-group -lc -lm -lgcc -leos -Wl,--end-group -T$(FE310_HOME)/default.lds
