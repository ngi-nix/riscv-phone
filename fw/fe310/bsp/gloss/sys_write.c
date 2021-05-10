#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#include "platform.h"

#define PUTC(c)     { while (UART0_REG(UART_REG_TXFIFO) & 0x80000000); UART0_REG(UART_REG_TXFIFO) = (c); }

/* Write to a file.  */
ssize_t
_write(int fd, const void *ptr, size_t len)
{
  if ((fd != STDOUT_FILENO) && (fd != STDERR_FILENO)) {
    errno = ENOSYS;
    return -1;
  }

  const char *current = ptr;
  for (size_t i = 0; i < len; i++) {
    if (current[i] == '\n') PUTC('\r');
    PUTC(current[i]);
  }
  return len;
}
