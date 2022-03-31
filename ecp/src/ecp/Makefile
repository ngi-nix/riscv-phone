include common.mk

obj += core.o timer.o
subdirs += crypto $(platform_dir)
build_dir = ../../build-$(platform)


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
	mkdir -p $(build_dir)
	rm -f $(build_dir)/*.a
	install libecp.a $(build_dir)
	install crypto/libecpcr.a $(build_dir)
	install $(platform_dir)/libecptm.a $(build_dir)
	install $(platform_dir)/libecptr.a $(build_dir)
	if [ -f htable/libecpht.a ]; then \
		install htable/libecpht.a $(build_dir); \
	fi
	if [ -f vconn/libecpvconn.a ]; then \
		install vconn/libecpvconn.a $(build_dir); \
	fi
	if [ -f ext/libecpext.a ]; then \
		install ext/libecpext.a $(build_dir); \
	fi
	if [ -f dir/libecpdir.a ]; then \
		install dir/libecpdir.a $(build_dir); \
	fi

clean:
	for i in $(subdirs); do \
		(cd $$i && $(MAKE) clean) || exit; \
	done
	rm -f *.o *.a