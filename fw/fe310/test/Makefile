include ../common.mk

CFLAGS += -I.. -I../bsp/include -I../bsp/drivers
LDFLAGS = $(CFLAGS) -L.. -Wl,--gc-sections -nostartfiles -nostdlib -Wl,--start-group -lc -lm -lgcc -leos -Wl,--end-group -T../bsp/default.lds

TARGET = modem

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

$(TARGET): $(TARGET).o
	$(CC) $< $(LDFLAGS) -o $@

clean:
	rm -f *.o *.a $(TARGET)

upload: $(TARGET)
	../bsp/upload --elf ./$(TARGET) --openocd $(RISCV_OPENOCD_HOME)/bin/openocd --gdb $(RISCV_HOME)/bin/riscv64-unknown-elf-gdb --openocd-config ../bsp/openocd.cfg
