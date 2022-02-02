include ../../ssl_common.mk
CFLAGS += -D__BEGIN_HIDDEN_DECLS= -D__END_HIDDEN_DECLS=

obj = sha1dgst.o sha1_one.o sha256.o sha512.o


all: $(obj)
dep: all

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.a
