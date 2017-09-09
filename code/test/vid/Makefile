# LIBVPX_HOME=/home/majstor/libvpx
LIBVPX_HOME=/Users/majstor/src/libvpx
CFLAGS=-D_V4L2_KERNEL_ -I/usr/src/linux-headers-$(uname -r) -I$(LIBVPX_HOME) -I../../core -I../../vconn -I../../util
LDFLAGS=-L$(LIBVPX_HOME)
dep=../../core/libecpcore.a ../../core/crypto/libecpcr.a ../../core/htable/libecpht.a ../../core/posix/libecptr.a ../../core/posix/libecptm.a ../../vconn/libecpvconn.a ../../util/libecputil.a ../init.o

all: client cap

%.o: %.c
	$(CC) $(CFLAGS) -c $<

cap: cap.o enc.o tools.o server.o 
	$(CC) -o $@ $< enc.o tools.o server.o -lvpx -lm -pthread $(dep) $(LDFLAGS)
	
client: client.o tools.o
	$(CC) -o $@ $< tools.o -lvpx -lm -pthread $(dep) $(LDFLAGS)

clean:
	rm -f *.o
	rm -f cap client

