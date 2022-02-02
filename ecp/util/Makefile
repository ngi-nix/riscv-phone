src_dir = ../src
include $(src_dir)/ecp/common.mk

obj=util.o
dep=../build-posix/*.a ./libecputil.a


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
