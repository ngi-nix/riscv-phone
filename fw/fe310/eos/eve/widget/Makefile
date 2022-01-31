include ../../../common.mk
CFLAGS += -I..

obj = label.o widget.o freew.o spacerw.o pagew.o strw.o textw.o selectw.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o
