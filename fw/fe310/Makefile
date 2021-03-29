include common.mk

CFLAGS += -I./include

subdirs = bsp/drivers bsp/gloss bsp/metal eos eos/i2c eos/eve eos/eve/screen eos/eve/widget eos/app

all: libeos.a

%.o: %.c
	$(CC) $(CFLAGS) -c $<

libeos.a:
	for i in $(subdirs); do \
		(cd $$i && $(MAKE)) || exit; \
	done
	rm -f $@
	for i in $(subdirs); do \
		$(AR) rcs $@ $$i/*.o; \
	done

clean:
	for i in $(subdirs); do \
		(cd $$i && $(MAKE) clean) || exit; \
	done
	rm -f *.o *.a
