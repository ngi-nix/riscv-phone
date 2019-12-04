include ../common.mk
CFLAGS += -I../include -include sys/cdefs.h

%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: plic_driver.o prci_driver.o

clean:
	rm -f *.o
