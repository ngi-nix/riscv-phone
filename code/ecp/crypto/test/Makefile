include ../../Makefile.platform
CFLAGS += -I.. -I../include

dep=../e_chacha20poly1305.o ../curve25519/*.o ../chacha/*.o ../poly1305/*.o ../sha/*.o ../compat/*.o


%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: aead aead_enc aead_dec ed25519 ed25519_sign ed25519_open

aead: aead.o
	$(CC) $(LDFLAGS) -o $@ $< $(dep)

aead_enc: aead_enc.o
	$(CC) $(LDFLAGS) -o $@ $< $(dep)

aead_dec: aead_dec.o
	$(CC) $(LDFLAGS) -o $@ $< $(dep)

ed25519: ed25519.o
	$(CC) $(LDFLAGS) -o $@ $< $(dep)

ed25519_sign: ed25519_sign.o
	$(CC) $(LDFLAGS) -o $@ $< $(dep)

ed25519_open: ed25519_open.o
	$(CC) $(LDFLAGS) -o $@ $< $(dep)


clean:
	rm -f *.o
	rm -f aead aead_enc aead_dec ed25519 ed25519_sign ed25519_open
