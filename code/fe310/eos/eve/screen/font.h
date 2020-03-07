#include <stdint.h>

typedef struct EVEFont {
    uint8_t id;
    uint8_t w[128];
    uint8_t h;
} EVEFont;

void eve_font_init(EVEFont *font, uint8_t font_id);