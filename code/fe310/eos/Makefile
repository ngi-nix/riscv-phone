include ../../ecp/Makefile.fe310

FE310_HOME = /opt/my/freedom-e-sdk

CC = $(FE310_HOME)/work/build/riscv-gnu-toolchain/riscv64-unknown-elf/prefix/bin/riscv64-unknown-elf-gcc
AR = $(FE310_HOME)/work/build/riscv-gnu-toolchain/riscv64-unknown-elf/prefix/bin/riscv64-unknown-elf-ar

CFLAGS = $(CFLAGS_PL) -I../..
 
obj = trap_entry.o eos.o msgq.o event.o interrupt.o timer.o i2s.o net.o ecp.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	$(AR) rcs libeos.a $(obj)

clean:
	rm -f *.o *.a