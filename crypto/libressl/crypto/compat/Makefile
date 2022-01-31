include ssl_common.mk

obj = arc4random.o explicit_bzero.o timingsafe_bcmp.o timingsafe_memcmp.o


all: $(obj)
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.a
