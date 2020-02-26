include ../../../common.mk

CFLAGS += -I.. -I../.. -I../../../include -I../../../drivers

obj = font.o text.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o