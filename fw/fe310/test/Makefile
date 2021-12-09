include ../common.mk

CRYPTO_DIR = ../../../crypto

CFLAGS += -I.. -I../eos -I../bsp/include -I../bsp/drivers -I$(CRYPTO_DIR)
LDFLAGS = $(CFLAGS) -L.. -Wl,--gc-sections -nostartfiles -nostdlib -Wl,--start-group -lc -lm -lgcc -leos -Wl,--end-group -T../bsp/default.lds

DEPS = main.o status.o cell_dev.o cell_pdp.o phone.o modem.o wifi.o cam.o audio.o test.o
# DEPS = main.o status.o cell_dev.o cell_pdp.o phone.o modem.o wifi.o cam.o fs.o test.o
TARGET = phone

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

$(TARGET): $(DEPS)
	$(CC) $(DEPS) $(LDFLAGS) -o $@

clean:
	rm -f *.o *.a $(TARGET)

upload: $(TARGET)
	../bsp/upload --elf ./$(TARGET) --openocd $(RISCV_OPENOCD_HOME)/bin/openocd --gdb $(RISCV_HOME)/bin/riscv64-unknown-elf-gdb --openocd-config ../bsp/openocd.cfg

