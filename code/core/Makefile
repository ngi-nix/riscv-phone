MAKE=make
CFLAGS = -I. -pthread -O3 -DECP_DEBUG
obj = core.o timer.o msgq.o
subdirs = crypto posix htable

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