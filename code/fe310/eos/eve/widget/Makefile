include ../../../common.mk

CFLAGS += -I. -I.. -I../..

obj = widget.o page.o text.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o