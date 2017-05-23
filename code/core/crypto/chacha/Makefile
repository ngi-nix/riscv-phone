CFLAGS=-I../include -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS= -O3 $(PIC)
obj = chacha.o

all: libchacha.a
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

libchacha.a: $(obj)
	$(AR) rcs $@ $(obj)

clean:
	rm -f *.o *.a
