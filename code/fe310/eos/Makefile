include ../common.mk

CFLAGS += -I../include -I../drivers

obj = trap_entry.o eos.o msgq.o event.o interrupt.o timer.o i2s.o uart.o spi.o net.o wifi.o cell.o sock.o eve.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o