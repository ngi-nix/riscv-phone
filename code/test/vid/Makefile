include ../../ecp/Makefile.platform

LIBVPX_HOME=/opt/my/libvpx
CFLAGS=$(CFLAGS_PL) -D_V4L2_KERNEL_ -I/usr/src/linux-headers-$(uname -r) -I$(LIBVPX_HOME) -I/opt/local/include/SDL2 -I../../ecp -I../../util -Wno-int-to-void-pointer-cast
LDFLAGS=$(LDFLAGS_PL) -L$(LIBVPX_HOME) -L/opt/local/lib 
dep=../init_vconn.o ../../ecp/build-posix/*.a ../../util/libecputil.a

all: cap

%.o: %.c
	$(CC) $(CFLAGS) -c $<

cap: cap.o enc.o tools.o server.o 
	$(CC) -o $@ $< enc.o tools.o server.o $(dep) -lvpx $(LDFLAGS)
	
client: client.o display.o tools.o
	$(CC) -o $@ $< display.o tools.o $(dep) $(LDFLAGS) -lvpx -lSDL2

clean:
	rm -f *.o
	rm -f cap client

