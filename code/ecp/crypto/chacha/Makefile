include ../../../Makefile.platform
CFLAGS=$(CFLAGS_PL) $(PIC) -I../include -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS=

obj = chacha.o


all: libchacha.a
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

libchacha.a: $(obj)
	$(AR) rcs $@ $(obj)

clean:
	rm -f *.o *.a
