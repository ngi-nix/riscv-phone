include ../ecp/Makefile.platform
CFLAGS=$(CFLAGS_PL) -I../ecp
LDFLAGS=$(LDFLAGS_PL)

obj=util.o
dep=../ecp/build-posix/*.a ./libecputil.a 


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
