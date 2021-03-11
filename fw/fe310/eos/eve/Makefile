include ../../common.mk

CFLAGS += -I.. -I../../bsp/include

obj = eve.o eve_platform.o eve_touch.o eve_vtrack.o eve_kbd.o eve_text.o eve_font.o clipb.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o
