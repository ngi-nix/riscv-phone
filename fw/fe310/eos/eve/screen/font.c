#include <stdlib.h>

#include "eve.h"
#include "unicode.h"

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

uint8_t eve_font_ch_w(EVEFont *font, utf32_t ch) {
    if (ch < 128) return font->w_ch[ch];
    return 0;
}

uint16_t eve_font_str_w(EVEFont *font, utf8_t *str) {
    uint16_t r = 0;
    utf32_t ch;
    uint8_t ch_w;
    uint8_t ch_l;

    if (str == NULL) return 0;

    while (*str) {
        ch_l = utf8_dec(str, &ch);
        ch_w = eve_font_ch_w(font, ch);
        r += ch_w;
        str += ch_l;
    }

    return r;
}

uint16_t eve_font_buf_w(EVEFont *font, utf8_t *buf, uint16_t buf_len) {
    int i = 0;
    uint16_t r = 0;
    utf32_t ch;
    uint8_t ch_w;
    uint8_t ch_l;

    while (i < buf_len) {
        ch_l = utf8_dec(buf + i, &ch);
        ch_w = eve_font_ch_w(font, ch);
        r += ch_w;
        i += ch_l;
    }

    return r;
}

uint8_t eve_font_h(EVEFont *font) {
    return font->h;
}