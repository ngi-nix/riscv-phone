include platform.mk
CFLAGS += -I.

obj = core.o timer.o dir.o dir_srv.o
subdirs = crypto ../platform/$(platform)

include ../platform/$(platform)_obj.mk

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	$(AR) rcs libecp.a $(obj)
	for i in $(subdirs); do \
		(cd $$i && $(MAKE)) || exit; \
	done

install: all
	mkdir -p build-$(platform)/
	install libecp.a build-$(platform)/
	install crypto/libecpcr.a build-$(platform)/
	install ../platform/$(platform)/libecptm.a build-$(platform)/
	install ../platform/$(platform)/libecptr.a build-$(platform)/
	if [ -f htable/libecpht.a ]; then \
		install htable/libecpht.a build-$(platform)/; \
	fi
	if [ -f vconn/libecpvconn.a ]; then \
		install vconn/libecpvconn.a build-$(platform)/; \
	fi

clean:
	for i in $(subdirs); do \
		(cd $$i && $(MAKE) clean) || exit; \
	done
	rm -f *.o *.a