CFLAGS=-I../core -I../vconn -I../util -O3 -Wno-int-to-void-pointer-cast
LDFLAGS=-lm -pthread

dep=../core/libecpcore.a ../core/crypto/libecpcr.a ../core/htable/libecpht.a ../core/posix/libecptr.a ../core/posix/libecptm.a ../vconn/libecpvconn.a ../util/libecputil.a

%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: basic client server echo stress vcs vc_server vc_client

basic: basic.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

client: client.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

server: server.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

echo: echo.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

stress: stress.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

vcs: vcs.o init_vconn.o $(dep)
	$(CC) -o $@ $< init_vconn.o $(dep) $(LDFLAGS)

vc_server: vc_server.o init_vconn.o $(dep)
	$(CC) -o $@ $< init_vconn.o $(dep) $(LDFLAGS)

vc_client: vc_client.o init_vconn.o $(dep)
	$(CC) -o $@ $< init_vconn.o $(dep) $(LDFLAGS)


opus_root=../../../opus-1.1.5
voip.o: voip.c
	$(CC) $(CFLAGS) -I $(opus_root)/include -c $<

voip: voip.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(opus_root)/.libs/libopus.a -lm $(LDFLAGS) -l asound 

clean:
	rm -f *.o
	rm -f basic client server echo stress vcs vc_server vc_client voip
