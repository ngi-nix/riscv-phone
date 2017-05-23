CFLAGS=-I../core -I../proxy -I../util -O3 -Wno-int-to-void-pointer-cast
LDFLAGS=-lm -pthread

dep=../core/libecpcore.a ../core/crypto/libecpcr.a ../core/htable/libecpht.a ../core/posix/libecptr.a ../core/posix/libecptm.a ../proxy/libecpproxy.a ../util/libecputil.a

%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: client server basic stress proxy pr_server pr_client

client: client.o init.o $(dep)
	$(CC) -o $@ $(LDFLAGS) $< init.o $(dep)

server: server.o init.o $(dep)
	$(CC) -o $@ $(LDFLAGS) $< init.o $(dep)

basic: basic.o init.o $(dep)
	$(CC) -o $@ $(LDFLAGS) $< init.o $(dep)

stress: stress.o init.o $(dep)
	$(CC) -o $@ $(LDFLAGS) $< init.o $(dep)

proxy: proxy.o init_proxy.o $(dep)
	$(CC) -o $@ $(LDFLAGS) $< init_proxy.o $(dep)

pr_server: pr_server.o init_proxy.o $(dep)
	$(CC) -o $@ $(LDFLAGS) $< init_proxy.o $(dep)

pr_client: pr_client.o init_proxy.o $(dep)
	$(CC) -o $@ $(LDFLAGS) $< init_proxy.o $(dep)

clean:
	rm -f *.o
	rm -f basic stress client server proxy pr_server pr_client
