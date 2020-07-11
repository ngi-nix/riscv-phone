#include <stdint.h>

#define EVE_CLIPB_SIZE_BUF  256

int eve_clipb_push(char *str, uint16_t len);
char *eve_clipb_get(void);