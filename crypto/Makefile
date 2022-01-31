include common.mk
include crypto_obj.mk

all:
	for i in $(subdirs); do \
		(cd $$i && $(MAKE)) || exit; \
	done

clean:
	for i in $(subdirs); do \
		(cd $$i && $(MAKE) clean) || exit; \
	done
	rm -f *.o *.a
