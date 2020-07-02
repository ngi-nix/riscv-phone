include ../../../common.mk

CFLAGS += -I. -I.. -I../..

obj = label.o widget.o page.o str.o text.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o