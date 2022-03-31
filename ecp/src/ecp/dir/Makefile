include ../common.mk

obj = dir.o

ifeq ($(with_dirsrv),yes)
obj += dir_srv.o
endif


%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	$(AR) rcs libecpdir.a $(obj)

clean:
	rm -f *.o *.a
