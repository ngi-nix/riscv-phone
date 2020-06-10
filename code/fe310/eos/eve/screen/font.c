#include <stdlib.h>

#include "eve.h"
#include "font.h"

void eve_font_init(EVEFont *font, uint8_t font_id) {
    font->id = font_id;
}

uint16_t eve_font_string_width(EVEFont *font, char *s) {
    uint16_t r = 0;

    while (*s) {
        r += font->w[*s];
        s++;
    }
    return r;
}

uint8_t eve_font_height(EVEFont *font) {
    return font->h;
}