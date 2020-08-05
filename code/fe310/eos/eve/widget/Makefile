include ../../../common.mk

CFLAGS += -I.. -I../..

obj = clipb.o label.o widget.o pagew.o strw.o textw.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o