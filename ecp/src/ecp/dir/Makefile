include ../common.mk

obj = dir.o dir_srv.o


%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	$(AR) rcs libecpdir.a $(obj)

clean:
	rm -f *.o *.a
