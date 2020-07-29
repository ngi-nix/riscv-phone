include ../../common.mk

CFLAGS += -I.. -I../../include

obj = eve.o eve_touch.o eve_track.o eve_kbd.o eve_text.o eve_platform.o unicode.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o