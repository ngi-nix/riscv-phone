include ../common.mk

CFLAGS += -I. -I../bsp/include -I../bsp/drivers

obj = trap_entry.o eos.o msgq.o event.o interrupt.o timer.o power.o i2s.o uart.o spi.o spi_dev.o net.o wifi.o cell.o sock.o unicode.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o
