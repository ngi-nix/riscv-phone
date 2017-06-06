CFLAGS = -I../core -pthread -O3 $(PIC)
LDFLAGS=-lm -pthread

obj=util.o
dep=../core/libecpcore.a ../core/crypto/libecpcr.a ../core/htable/libecpht.a ../core/posix/libecptr.a ../core/posix/libecptm.a ./libecputil.a 


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

all: libecputil.a mknode
	
libecputil.a: $(obj)
	$(AR) rcs libecputil.a $(obj)

mknode: mknode.o libecputil.a
	$(CC) -o $@ $< $(dep) $(LDFLAGS)

clean:
	rm -f *.o *.a
	rm -f mknode
