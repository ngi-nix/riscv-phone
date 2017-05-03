CFLAGS=-I.. -O3 -Wno-int-to-void-pointer-cast
LDFLAGS=-lm -pthread
dep=../libecpcore.a ../crypto/libecpcr.a ../htable/libecpht.a ../posix/libecptr.a ../posix/libecptm.a

%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: server stress

server: server.o $(dep)
	$(CC) $< $(dep) $(LDFLAGS) -o $@

stress: stress.o $(dep)
	$(CC) $< $(dep) $(LDFLAGS) -o $@

clean:
	rm -f *.o
	rm -f server stress
