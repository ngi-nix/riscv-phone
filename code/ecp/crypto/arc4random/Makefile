include ../../Makefile.platform
CFLAGS=$(CFLAGS_PL) $(PIC)

obj = arc4random.o


all: $(obj)
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.a
