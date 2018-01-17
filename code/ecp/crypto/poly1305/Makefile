include ../../Makefile.platform
CFLAGS=$(CFLAGS_PL) $(PIC) -I../include -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS=

obj = poly1305.o


all: $(obj)
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.a
