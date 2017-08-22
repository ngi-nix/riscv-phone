LIBVPX_HOME=../libvpx
CFLAGS=-D_V4L2_KERNEL_ -I/usr/src/linux-headers-$(uname -r) -I$(LIBVPX_HOME)
LDFLAGS=-L$(LIBVPX_HOME)
deps=$(LIBVPX_HOME)/ivfenc.c.o $(LIBVPX_HOME)/video_writer.c.o

all: cap

%.o: %.c
	$(CC) $(CFLAGS) -c $<

cap: cap.o enc.o tools.o
	$(CC) -o $@ $< enc.o tools.o $(deps) -lvpx -lm -pthread $(LDFLAGS)
	
clean:
	rm -f *.o
	rm -f cap

