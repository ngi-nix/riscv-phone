include ../../common.mk
CFLAGS += -I$(bsp_dir)/include

src = sys_access.c sys_chdir.c sys_chmod.c sys_chown.c sys_close.c sys_execve.c sys_exit.c sys_faccessat.c sys_fork.c sys_fstat.c sys_fstatat.c sys_ftime.c sys_getcwd.c sys_getpid.c \
	sys_isatty.c sys_kill.c sys_link.c sys_lseek.c sys_lstat.c sys_open.c sys_openat.c sys_read.c sys_sbrk.c sys_stat.c sys_sysconf.c sys_unlink.c sys_utime.c sys_wait.c sys_write.c
obj = crt0.o $(src:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o
