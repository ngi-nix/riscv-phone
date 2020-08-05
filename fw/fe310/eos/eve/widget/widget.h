#include <stdint.h>

#define EVE_WIDGET_TYPE_PAGE    1
#define EVE_WIDGET_TYPE_STR     2
#define EVE_WIDGET_TYPE_TEXT    3

struct EVEWidget;

typedef int (*eve_widget_touch_t) (struct EVEWidget *, EVEPage *, uint8_t, int);
typedef uint8_t (*eve_widget_draw_t) (struct EVEWidget *, EVEPage *, uint8_t);

typedef struct EVEWidget {
    EVERect g;
    eve_widget_touch_t touch;
    eve_widget_draw_t draw;
    eve_kbd_input_handler_t putc;
    EVELabel *label;
    uint8_t type;
} EVEWidget;

void eve_widget_init(EVEWidget *widget, uint8_t type, EVERect *g, eve_widget_touch_t touch, eve_widget_draw_t draw, eve_kbd_input_handler_t putc);
void eve_widget_set_label(EVEWidget *widget, EVELabel *label);
EVEWidget *eve_widget_next(EVEWidget *widget);
