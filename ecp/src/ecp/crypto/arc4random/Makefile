include ../../Makefile.platform
CFLAGS += $(PIC)

obj = arc4random.o


all: $(obj)
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.a
