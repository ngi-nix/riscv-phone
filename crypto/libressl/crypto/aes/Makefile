include ../../ssl_common.mk
CFLAGS+= -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS=

obj = aes_core.o aes_ecb.o aes_cbc.o aes_cfb.o aes_ctr.o aes_ige.o aes_ofb.o


all: $(obj)
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.a
