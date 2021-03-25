#include <stdint.h>

#define EVE_WIDGET_TYPE_FREE    1
#define EVE_WIDGET_TYPE_SPACER  2
#define EVE_WIDGET_TYPE_PAGE    3
#define EVE_WIDGET_TYPE_STR     4
#define EVE_WIDGET_TYPE_TEXT    5
#define EVE_WIDGET_TYPE_SELECT  6

struct EVEWidget;

typedef uint8_t (*eve_widget_draw_t) (struct EVEWidget *, uint8_t);
typedef int (*eve_widget_touch_t) (struct EVEWidget *, EVETouch *, uint16_t);

typedef struct EVEWidget {
    EVERect g;
    EVEPage *page;
    eve_widget_draw_t draw;
    eve_widget_touch_t touch;
    eve_kbd_input_handler_t putc;
    EVELabel *label;
    uint8_t type;
    uint8_t tag0;
    uint8_t tagN;
} EVEWidget;

void eve_widget_init(EVEWidget *widget, uint8_t type, EVERect *g, EVEPage *page, eve_widget_draw_t draw, eve_widget_touch_t touch, eve_kbd_input_handler_t putc);
size_t eve_widget_size(uint8_t type);
void eve_widget_set_label(EVEWidget *widget, EVELabel *label);
EVEWidget *eve_widget_next(EVEWidget *widget);

void eve_widget_uievt_push(EVEWidget *widget, uint16_t evt, void *param);