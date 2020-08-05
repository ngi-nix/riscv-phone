include ../../Makefile.platform
CFLAGS += $(PIC) -I../include -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS=

obj = explicit_bzero.o timingsafe_memcmp.o timingsafe_bcmp.o


all: $(obj)
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.a
