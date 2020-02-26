#include <stdint.h>

#include "eve_kbd.h"

#define EVE_WIDGET_TYPE_TEXT    1

struct EVEScreen;

typedef struct EVEWidget {
    uint8_t type;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    int (*touch) (struct EVEWidget *, struct EVEScreen *, uint8_t, int);
    uint8_t (*draw) (struct EVEWidget *, struct EVEScreen *, uint8_t, char);
    eve_kbd_input_handler_t putc;
} EVEWidget;

