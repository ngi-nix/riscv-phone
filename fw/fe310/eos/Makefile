include ../common.mk

CRYPTO_DIR = ../../../crypto

CFLAGS += -I. -I../bsp/include -I../bsp/drivers -I$(CRYPTO_DIR)

obj = trap_entry.o eos.o msgq.o event.o interrupt.o timer.o power.o i2s.o i2c.o uart.o spi.o spi_dev.o sdcard.o sdc_crypto.o cam.o net.o wifi.o cell.o sock.o unicode.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o
