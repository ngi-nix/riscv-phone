include Makefile.platform
CFLAGS += $(PIC) -I.

obj = core.o timer.o $(obj_rbuf)
subdirs = crypto platform $(htable) $(vconn)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	$(AR) rcs libecpcore.a $(obj)
	for i in $(subdirs); do \
		(cd $$i && $(MAKE)) || exit; \
	done

install: all
	mkdir -p build-$(platform)/
	install libecpcore.a build-$(platform)/
	install crypto/libecpcr.a build-$(platform)/
	install platform/libecptm.a build-$(platform)/
	install platform/libecptr.a build-$(platform)/
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