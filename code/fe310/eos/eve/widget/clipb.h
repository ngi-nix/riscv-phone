#include <stdint.h>

#define EVE_CLIPB_SIZE_BUF  256

int eve_clipb_push(uint8_t *str, uint16_t len);
uint8_t *eve_clipb_get(void);