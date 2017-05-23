CFLAGS=-I../include -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS= -O3 $(PIC)

getentropy = getentropy_osx
obj = explicit_bzero.o timingsafe_memcmp.o timingsafe_bcmp.o # arc4random.o arc4random_uniform.o $(getentropy).o


all: libcompat.a
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

libcompat.a: $(obj)
	$(AR) rcs $@ $(obj)

clean:
	rm -f *.o *.a
