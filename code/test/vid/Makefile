LIBVPX_HOME=/opt/my/libvpx
CFLAGS=-D_V4L2_KERNEL_ -I/usr/src/linux-headers-$(uname -r) -I$(LIBVPX_HOME) -I/opt/local/include/SDL2 -I../../core -I../../vconn -I../../util
LDFLAGS=-L$(LIBVPX_HOME) -L/opt/local/lib
dep=../../ecp/build-posix/*.a ../../util/libecputil.a ../init.o

all: client cap

%.o: %.c
	$(CC) $(CFLAGS) -c $<

cap: cap.o enc.o tools.o server.o 
	$(CC) -o $@ $< enc.o tools.o server.o -lvpx -lm -pthread $(dep) $(LDFLAGS)
	
client: client.o display.o tools.o
	$(CC) -o $@ $< display.o tools.o -lvpx -lSDL2 -lm -pthread $(dep) $(LDFLAGS)

clean:
	rm -f *.o
	rm -f cap client

