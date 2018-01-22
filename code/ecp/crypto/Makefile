include ../Makefile.platform
CFLAGS=$(CFLAGS_PL) $(PIC) -Iinclude -I.. -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS=

obj = e_chacha20poly1305.o crypto.o
obj_dep = compat/explicit_bzero.o compat/timingsafe_memcmp.o compat/timingsafe_bcmp.o \
	chacha/chacha.o poly1305/poly1305.o curve25519/curve25519.o curve25519/curve25519-generic.o \
	sha/sha256.o sha/sha512.o arc4random/arc4random.o
subdirs = compat curve25519 chacha poly1305 sha arc4random


%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	for i in $(subdirs); do \
		(cd $$i && $(MAKE) && cd ..) || exit; \
	done
	$(AR) rcs libecpcr.a $(obj) $(obj_dep)
	
clean:
	for i in $(subdirs) test; do \
		(cd $$i && $(MAKE) clean && cd ..) || exit; \
	done
	rm -f *.o *.a
