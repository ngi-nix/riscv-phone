include ../../../common.mk

CFLAGS += -I.. -I../..

obj = font.o screen.o window.o kbdwin.o page.o form.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o