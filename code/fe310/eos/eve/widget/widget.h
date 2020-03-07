#include <stdint.h>

#define EVE_WIDGET_TYPE_TEXT    1
#define EVE_WIDGET_TYPE_PAGE    2

struct EVEWidget;

typedef int (*eve_widget_touch_t) (struct EVEWidget *, EVEPage *, uint8_t, int, EVEPageFocus *);
typedef uint8_t (*eve_widget_draw_t) (struct EVEWidget *, EVEPage *, uint8_t);

typedef struct EVEWidget {
    uint8_t type;
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    eve_widget_touch_t touch;
    eve_widget_draw_t draw;
    eve_kbd_input_handler_t putc;
} EVEWidget;

void eve_widget_init(EVEWidget *widget, uint8_t type, int16_t x, int16_t y, uint16_t w, uint16_t h, eve_widget_touch_t touch, eve_widget_draw_t draw, eve_kbd_input_handler_t putc);
EVEWidget *eve_widget_next(EVEWidget *widget);
