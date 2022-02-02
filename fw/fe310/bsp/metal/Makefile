include ../../common.mk
CFLAGS += -I$(bsp_dir)/include -I$(bsp_dir)/drivers

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: entry.o metal.o

clean:
	rm -f *.o
