include ../../common.mk
ssl_dir = ../../../../../ext/libressl
CFLAGS += -I.. -I$(ssl_dir)/include

dep=../libecpcr.a


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
