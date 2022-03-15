include ../common.mk

obj = ext.o frag.o rbuf.o rbuf_send.o rbuf_recv.o msgq.o


%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	$(AR) rcs libecpext.a $(obj)

clean:
	rm -f *.o *.a
