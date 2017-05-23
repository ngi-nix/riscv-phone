CFLAGS=-I../include -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS= -O3 $(PIC)

obj = sha1dgst.o sha1_one.o sha256.o sha512.o


all: libsha.a
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

libsha.a: $(obj)
	$(AR) rcs $@ $(obj)

clean:
	rm -f *.o *.a
