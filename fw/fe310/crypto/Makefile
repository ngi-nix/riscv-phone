include ../common.mk

CRYPTO_DIR = ../../../crypto

obj = $(CRYPTO_DIR)/aes/aes.o $(CRYPTO_DIR)/blowfish/blowfish.o $(CRYPTO_DIR)/sha/sha1.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o $(obj)
