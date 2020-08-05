include ../../common.mk
CFLAGS += -I../include -I../drivers

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: entry.o metal.o

clean:
	rm -f *.o
