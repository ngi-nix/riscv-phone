include ../../Makefile.platform
CFLAGS=$(CFLAGS_PL) $(PIC) -I.. -std=gnu89 

obj = htable.o hashtable.o hashtable_itr.o


%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	$(AR) rcs libecpht.a $(obj)

clean:
	rm -f *.o *.a
