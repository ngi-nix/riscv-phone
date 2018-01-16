FE310_HOME=/opt/my/freedom-e-sdk

CC=$(FE310_HOME)/work/build/riscv-gnu-toolchain/riscv64-unknown-elf/prefix/bin/riscv64-unknown-elf-gcc
AR=$(FE310_HOME)/work/build/riscv-gnu-toolchain/riscv64-unknown-elf/prefix/bin/riscv64-unknown-elf-ar

CFLAGS=-O3 -fno-builtin-printf -march=rv32imac -mabi=ilp32 -mcmodel=medany -I$(FE310_HOME)/bsp/include -I$(FE310_HOME)/bsp/drivers -I$(FE310_HOME)/bsp/env -I$(FE310_HOME)/bsp/env/freedom-e300-hifive1 -I../..

obj = msgq.o event.o interrupt.o timer.o net.o eos.o ecp.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

all: $(obj)
	$(AR) rcs libeos.a $(obj)

clean:
	rm -f *.o *.a