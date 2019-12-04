include common.mk

CFLAGS += -I./include

subdirs = drivers gloss metal eos

all: libeos.a

%.o: %.c
	$(CC) $(CFLAGS) -c $<

libeos.a:
	for i in $(subdirs); do \
		(cd $$i && $(MAKE) && cd ..) || exit; \
	done
	rm -f $@
	for i in $(subdirs); do \
		$(AR) rcs $@ $$i/*.o; \
	done

clean:
	for i in $(subdirs); do \
		(cd $$i && $(MAKE) clean && cd ..) || exit; \
	done
	rm -f *.o *.a
