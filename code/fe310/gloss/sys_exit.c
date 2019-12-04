#include <stdlib.h>

__attribute__ ((noreturn)) void
_exit(int st) { while (1); }

int
atexit(void (*f)(void)) { return 0; }
