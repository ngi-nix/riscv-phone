CFLAGS=-I../core -O3 -Wno-int-to-void-pointer-cast
LDFLAGS=-lm -pthread
dep=../core/libecpcore.a ../core/crypto/libecpcr.a ../core/htable/libecpht.a ../core/posix/libecptr.a ../core/posix/libecptm.a init.o

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
