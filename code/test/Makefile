CFLAGS=-I../core -O3 -Wno-int-to-void-pointer-cast
LDFLAGS=-lm -pthread

dep=../core/libecpcore.a ../core/crypto/libecpcr.a ../core/htable/libecpht.a ../core/posix/libecptr.a ../core/posix/libecptm.a

%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: basic stress

basic: basic.o init.o $(dep)
	$(CC) -o $@ $(LDFLAGS) $< init.o $(dep)

stress: stress.o init.o $(dep)
	$(CC) -o $@ $(LDFLAGS) $< init.o $(dep)

clean:
	rm -f *.o
	rm -f basic stress
