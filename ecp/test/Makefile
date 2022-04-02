src_dir = ../src
include $(src_dir)/ecp/common.mk
CFLAGS += -I../util -Wno-int-to-void-pointer-cast

dep=../build-posix/*.a ../util/libecputil.a

%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: basic client server vcs vc_server vc_client

basic: basic.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

client: client.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

server: server.o init.o $(dep)
	$(CC) -o $@ $< init.o $(dep) $(LDFLAGS)

vcs: vcs.o init_vconn.o $(dep)
	$(CC) -o $@ $< init_vconn.o $(dep) $(LDFLAGS)

vc_server: vc_server.o init_vconn.o $(dep)
	$(CC) -o $@ $< init_vconn.o $(dep) $(LDFLAGS)

vc_client: vc_client.o init_vconn.o $(dep)
	$(CC) -o $@ $< init_vconn.o $(dep) $(LDFLAGS)

clean:
	rm -f *.o
	rm -f basic dir client server echo frag stress vcs vc_server vc_client vc_client_t voip
