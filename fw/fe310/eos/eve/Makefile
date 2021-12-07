include ../../common.mk

CFLAGS += -I.. -I../../bsp/include

obj = eve.o eve_eos.o eve_platform.o eve_touch.o eve_phy.o eve_vtrack.o eve_font.o eve_kbd.o eve_text.o clipb.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o
