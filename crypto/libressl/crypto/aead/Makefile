include ../../ssl_common.mk

obj = e_chacha20poly1305.o


all: $(obj)
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.a
