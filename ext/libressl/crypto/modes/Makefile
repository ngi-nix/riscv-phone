include ../../ssl_common.mk
CFLAGS += -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS=

obj = cbc128.o ccm128.o cfb128.o ctr128.o cts128.o gcm128.o ofb128.o xts128.o


all: $(obj)
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.a
