CFLAGS=-I.. -I../include -O3 
aead_dep=../compat/explicit_bzero.o ../compat/timingsafe_memcmp.o ../compat/timingsafe_bcmp.o \
	../chacha/chacha.o ../poly1305/poly1305.o ../curve25519/curve25519.o ../curve25519/curve25519-generic.o \
	../sha/sha512.o	../e_chacha20poly1305.o
dsa_dep=../*/*.a

%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: aead aead_enc aead_dec ed25519 ed25519_sign ed25519_open

aead: aead.o
	$(CC) $(LDFLAGS) -o $@ $< $(aead_dep)

aead_enc: aead_enc.o
	$(CC) $(LDFLAGS) -o $@ $< $(aead_dep)

aead_dec: aead_dec.o
	$(CC) $(LDFLAGS) -o $@ $< $(aead_dep)

ed25519: ed25519.o
	$(CC) $(LDFLAGS) -o $@ $< $(dsa_dep)

ed25519_sign: ed25519_sign.o
	$(CC) $(LDFLAGS) -o $@ $< $(dsa_dep)

ed25519_open: ed25519_open.o
	$(CC) $(LDFLAGS) -o $@ $< $(dsa_dep)


clean:
	rm -f *.o
	rm -f aead aead_enc aead_dec ed25519 ed25519_sign ed25519_open
