include ../../../common.mk

CFLAGS += -I. -I.. -I../.. -I../../../include -I../../../drivers

obj = font.o screen.o tile.o page.o form.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o