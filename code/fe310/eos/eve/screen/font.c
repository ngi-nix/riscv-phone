#include <stdlib.h>

#include "eve.h"
#include "font.h"

void eve_font_init(EVEFont *font, uint8_t font_id) {
    font->id = font_id;
}