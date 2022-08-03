include ../common.mk
CFLAGS += -I$(bsp_dir)/include -I$(bsp_dir)/drivers -I$(ext_dir)/crypto -I$(ext_dir)/fsfat -I$(ecp_dir)/src -I$(ecp_dir)/src/platform/fe310 -I..
LDFLAGS = app/*.o $(CFLAGS) -L.. -L$(ecp_dir)/build-fe310 -Wl,--gc-sections -nostartfiles -nostdlib -Wl,--start-group -lc -lm -lgcc -leos -lecp -lecpcr -lecptr -lecptm -lecpdir -lecpvconn -Wl,--end-group -T../bsp/default.lds

DEPS = main.o mem.o cell_dev.o cell_pdp.o phone.o modem.o wifi.o cam.o fs.o test.o
# DEPS += ecp.o ecp_init.o audio.o
TARGET = phone


all: $(TARGET)

app_:
	(cd app && $(MAKE)) || exit;

%.o: %.c
	riscv64-none-elf-gcc $(CFLAGS) -c $<

$(TARGET): app_ $(DEPS)
	riscv64-none-elf-gcc $(DEPS) $(LDFLAGS) -o $@

clean:
	(cd app && $(MAKE) clean) || exit;
	rm -f *.o *.a $(TARGET)

upload: $(TARGET)
	../bsp/upload --elf ./$(TARGET) --openocd $(RISCV_OPENOCD_HOME)/bin/openocd --gdb $(RISCV_HOME)/bin/riscv64-unknown-elf-gdb --openocd-config ../bsp/openocd.cfg

