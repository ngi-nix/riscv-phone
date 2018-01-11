include ../Makefile.platform
CFLAGS=$(CFLAGS_PL) -I../ecp -I../util -Wno-int-to-void-pointer-cast
LDFLAGS=$(LDFLAGS_PL)

dep=../ecp/libecpcore.a ../ecp/crypto/libecpcr.a ../ecp/htable/libecpht.a ../ecp/posix/libecptr.a ../ecp/posix/libecptm.a ../ecp/vconn/libecpvconn.a ../util/libecputil.a

%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: basic client server echo frag stress vcs vc_server vc_client

basic: basic.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

client: client.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

server: server.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

echo: echo.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

frag: frag.o init.o $(dep)
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
	rm -f basic client server echo frag stress vcs vc_server vc_client voip
