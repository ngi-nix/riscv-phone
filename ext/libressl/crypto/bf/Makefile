include ../../ssl_common.mk
CFLAGS+= -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS=

obj = bf_skey.o bf_enc.o bf_ecb.o bf_cfb64.o bf_ofb64.o


all: $(obj)
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.a
