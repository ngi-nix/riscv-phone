include ../Makefile.platform
CFLAGS += $(PIC) -I..

obj = vconn.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	$(AR) rcs libecpvconn.a $(obj)

clean:
	rm -f *.o *.a