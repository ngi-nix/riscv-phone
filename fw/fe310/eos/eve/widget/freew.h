#include <stdint.h>

struct EVEFreeWidget;

typedef void (*eve_freew_draw_t) (struct EVEFreeWidget *);
typedef int (*eve_freew_touch_t) (struct EVEFreeWidget *, EVETouch *, uint16_t);

typedef struct EVEFreeWidget {
    EVEWidget w;
    eve_freew_draw_t _draw;
    eve_freew_touch_t _touch;
} EVEFreeWidget;

typedef struct EVEFreeSpec {
    eve_freew_draw_t draw;
    eve_freew_touch_t touch;
    eve_kbd_input_handler_t putc;
} EVEFreeSpec;

int eve_freew_create(EVEFreeWidget *widget, EVERect *g, EVEPage *page, EVEFreeSpec *spec);
void eve_freew_init(EVEFreeWidget *widget, EVERect *g, EVEPage *page, eve_freew_draw_t draw, eve_freew_touch_t touch, eve_kbd_input_handler_t putc);

void eve_freew_tag(EVEFreeWidget *widget);
uint8_t eve_freew_draw(EVEWidget *_widget, uint8_t tag0);
int eve_freew_touch(EVEWidget *_widget, EVETouch *touch, uint16_t evt);
