#include <stdint.h>

typedef uint8_t utf8_t;
typedef uint16_t utf16_t;
typedef uint32_t utf32_t;

uint8_t utf8_enc(utf32_t ch, utf8_t *str);
uint8_t utf8_dec(utf8_t *str, utf32_t *ch);
int utf8_seek(utf8_t *str, int off, utf32_t *ch);
int utf8_verify(utf8_t *str, int sz);