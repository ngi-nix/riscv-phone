#include <stdint.h>

struct EVEFreeWidget;

typedef int (*eve_freew_touch_t) (struct EVEFreeWidget *, EVEPage *, EVETouch *, uint16_t);
typedef void (*eve_freew_draw_t) (struct EVEFreeWidget *, EVEPage *);

typedef struct EVEFreeWidget {
    EVEWidget w;
    eve_freew_touch_t _touch;
    eve_freew_draw_t _draw;
} EVEFreeWidget;

typedef struct EVEFreeSpec {
    eve_freew_touch_t touch;
    eve_freew_draw_t draw;
    eve_kbd_input_handler_t putc;
} EVEFreeSpec;

int eve_freew_create(EVEFreeWidget *widget, EVERect *g, EVEFont *font, EVEFreeSpec *spec);
void eve_freew_init(EVEFreeWidget *widget, EVERect *g, EVEFont *font, eve_freew_touch_t touch, eve_freew_draw_t draw, eve_kbd_input_handler_t putc);
void eve_freew_update(EVEFreeWidget *widget, eve_freew_touch_t touch, eve_freew_draw_t draw, eve_kbd_input_handler_t putc);

void eve_freew_tag(EVEFreeWidget *widget);
int eve_freew_touch(EVEWidget *_widget, EVEPage *page, EVETouch *t, uint16_t evt);
uint8_t eve_freew_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0);
