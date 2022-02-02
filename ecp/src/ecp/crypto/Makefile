include ../common.mk
ssl_dir = ../../../../ext/libressl
CFLAGS += -I$(ssl_dir)/include
MAKEFLAGS += -I$(pwd)/..

include $(ssl_dir)/ssl_obj.mk
obj = crypto.o
obj_ssl = $(addprefix $(ssl_dir)/,$(obj_dep))
subdirs = $(ssl_dir)


%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	for i in $(subdirs); do \
		(cd $$i && $(MAKE)) || exit; \
	done
	$(AR) rcs libecpcr.a $(obj) $(obj_ssl)

clean:
	for i in $(subdirs); do \
		(cd $$i && $(MAKE) clean) || exit; \
	done
	rm -f *.o *.a
