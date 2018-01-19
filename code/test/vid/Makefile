include ../../ecp/Makefile.platform

LIBVPX_HOME=/opt/my/libvpx
CFLAGS=$(CFLAGS_PL) -D_V4L2_KERNEL_ -I/usr/src/linux-headers-$(uname -r) -I$(LIBVPX_HOME) -I/opt/local/include/SDL2 -I../../ecp -I../../util -Wno-int-to-void-pointer-cast
LDFLAGS=-L$(LIBVPX_HOME) -L/opt/local/lib $(LDFLAGS_PL)
dep=../../ecp/build-posix/*.a ../../util/libecputil.a ../init_vconn.o

all: cap

%.o: %.c
	$(CC) $(CFLAGS) -c $<

cap: cap.o enc.o tools.o server.o 
	$(CC) -o $@ $(LDFLAGS) -lvpx $< enc.o tools.o server.o $(dep)
	
client: client.o display.o tools.o
	$(CC) -o $@ $(LDFLAGS) -lvpx -lSDL2 $< display.o tools.o $(dep)

clean:
	rm -f *.o
	rm -f cap client

