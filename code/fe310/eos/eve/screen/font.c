#include <stdlib.h>

#include "eve.h"
#include "font.h"

void eve_font_init(EVEFont *font, uint8_t font_id) {
    uint32_t p;

    p = eve_read32(EVE_ROM_FONT_ADDR);
    p += (148 * (font_id - 16));
    font->id = font_id;
    font->w = eve_read32(p + 136);
    font->h = eve_read32(p + 140);
    eve_readb(p, font->w_ch, 128);
}

uint16_t eve_font_str_w(EVEFont *font, char *s) {
    uint16_t r = 0;

    while (*s) {
        r += font->w_ch[*s];
        s++;
    }

    return r;
}

uint16_t eve_font_buf_w(EVEFont *font, char *buf, uint16_t buf_len) {
    int i;
    uint16_t r = 0;

    for (i=0; i<buf_len; i++) {
        r += font->w_ch[*(buf + i)];
    }

    return r;
}

uint8_t eve_font_h(EVEFont *font) {
    return font->h;
}