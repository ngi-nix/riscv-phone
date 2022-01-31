include ../common.mk

obj_tr = transport.o
obj_tm = time.o


%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: libecptr.a libecptm.a

libecptr.a: $(obj_tr)
	$(AR) rcs libecptr.a $(obj_tr)

libecptm.a: $(obj_tm)
	$(AR) rcs libecptm.a $(obj_tm)

clean:
	rm -f *.o *.a
