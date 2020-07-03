#include <stdint.h>

typedef struct EVEFont {
    uint8_t id;
    uint8_t w;
    uint8_t h;
    uint8_t w_ch[128];
} EVEFont;

void eve_font_init(EVEFont *font, uint8_t font_id);
uint16_t eve_font_str_w(EVEFont *font, char *s);
uint16_t eve_font_buf_w(EVEFont *font, char *buf, uint16_t buf_len);
uint8_t eve_font_h(EVEFont *font);
