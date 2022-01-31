include common.mk
MAKEFLAGS += -I$(fe310_dir)

include $(crypto_dir)/crypto_obj.mk
obj_crypto = $(addprefix $(crypto_dir)/,$(obj_dep))

subdirs = bsp/drivers bsp/gloss bsp/metal eos eos/i2c eos/eve eos/eve/screen eos/eve/widget eos/app fsfat

# $(info INFO $(obj_crypto))
all: libeos.a

%.o: %.c
	$(CC) $(CFLAGS) -c $<

libeos.a:
	for i in $(subdirs) $(crypto_dir); do \
		(cd $$i && $(MAKE)) || exit; \
	done
	rm -f $@
	for i in $(subdirs); do \
		$(AR) rcs $@ $$i/*.o; \
	done
	$(AR) rcs $@ $(obj_crypto)

clean:
	for i in $(subdirs) $(crypto_dir); do \
		(cd $$i && $(MAKE) clean) || exit; \
	done
	rm -f *.o *.a
