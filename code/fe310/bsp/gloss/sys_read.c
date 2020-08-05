#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#include "platform.h"

/* Read from a file.  */
ssize_t
_read(int fd, void *ptr, size_t len)
{
  if (fd != STDIN_FILENO) {
    errno = ENOSYS;
    return -1;
  }

  char *current = ptr;
  for (size_t i = 0; i < len; i++) {
    volatile uint32_t r;
    while ((r = UART0_REG(UART_REG_RXFIFO)) & 0x80000000);
    current[i] = r & 0xFF;
  }
  return len;
}
