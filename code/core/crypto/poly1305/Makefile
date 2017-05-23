CFLAGS=-I../include -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS= -O3 $(PIC)
obj = poly1305.o

all: libpoly1305.a
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

libpoly1305.a: $(obj)
	$(AR) rcs $@ $(obj)

clean:
	rm -f *.o *.a
