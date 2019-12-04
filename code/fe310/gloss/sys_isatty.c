#include <unistd.h>

int
_isatty(int fd)
{
  return ((fd == STDIN_FILENO) || (fd == STDOUT_FILENO) || (fd == STDERR_FILENO));
}
