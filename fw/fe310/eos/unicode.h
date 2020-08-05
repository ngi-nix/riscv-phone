#include <stdint.h>

#define UTF_OK      0
#define UTF_ERR     -1

typedef uint8_t utf8_t;
typedef uint16_t utf16_t;
typedef uint32_t utf32_t;

int utf8_enc(utf32_t ch, utf8_t *str);
int utf8_dec(utf8_t *str, utf32_t *ch);
int utf8_seek(utf8_t *str, int off, utf32_t *ch);
int utf8_verify(utf8_t *str, int str_size, int *str_len);

int utf16_enc(utf32_t ch, uint8_t *str);
int utf16_dec(uint8_t *str, utf32_t *ch);
int utf16_seek(uint8_t *str, int off, utf32_t *ch);
