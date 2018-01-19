include ../../ecp/Makefile.platform

LIBVPX_HOME=/opt/my/libvpx
CFLAGS=$(CFLAGS_PL) -D_V4L2_KERNEL_ -I/usr/src/linux-headers-$(uname -r) -I$(LIBVPX_HOME) -I/opt/local/include/SDL2 -I../../ecp -I../../util -Wno-int-to-void-pointer-cast
LDFLAGS=-L$(LIBVPX_HOME) -L/opt/local/lib $(LDFLAGS_PL)
dep=../../ecp/build-posix/*.a ../../util/libecputil.a ../init_vconn.o

all: cap

%.o: %.c
	$(CC) $(CFLAGS) -c $<

cap: cap.o enc.o tools.o server.o 
	$(CC) -o $@ $< enc.o tools.o server.o $(dep) $(LDFLAGS) -lvpx -lm -pthread 
	
client: client.o display.o tools.o
	$(CC) -o $@ $< display.o tools.o $(dep) $(LDFLAGS) -lvpx -lSDL2 -lm -pthread 

clean:
	rm -f *.o
	rm -f cap client

