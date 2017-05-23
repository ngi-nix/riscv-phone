CFLAGS = -I../core -pthread -O3 $(PIC)

obj = proxy.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	$(AR) rcs libecpproxy.a $(obj)

clean:
	rm -f *.o *.a