include ../common.mk
CFLAGS += -I. -I$(bsp_dir)/include -I$(bsp_dir)/drivers -I$(ext_dir)/crypto

obj = trap_entry.o eos.o msgq.o event.o interrupt.o timer.o pwr.o i2s.o i2c.o uart.o spi.o spi_dev.o lcd.o sdcard.o sdc_crypto.o cam.o net.o wifi.o cell.o sock.o unicode.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o
