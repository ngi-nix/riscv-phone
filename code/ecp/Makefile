include ../Makefile.platform
CFLAGS=$(CFLAGS_PL) $(PIC) -I.

obj = core.o timer.o $(obj_rbuf)
subdirs = crypto $(vconn) $(htable) $(platform) 

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	$(AR) rcs libecpcore.a $(obj)
	for i in $(subdirs); do \
		(cd $$i && $(MAKE) && cd ..) || exit; \
	done

clean:
	for i in $(subdirs); do \
		(cd $$i && $(MAKE) clean && cd ..) || exit; \
	done
	rm -f *.o *.a