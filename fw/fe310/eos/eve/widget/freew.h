#include <stdint.h>

struct EVEFreeWidget;

typedef int (*eve_freew_touch_t) (struct EVEFreeWidget *, EVEPage *, EVETouch *, uint16_t, uint8_t, int);
typedef uint8_t (*eve_freew_draw_t) (struct EVEFreeWidget *, EVEPage *, uint8_t);

typedef struct EVEFreeWidget {
    EVEWidget w;
    eve_freew_touch_t _touch;
    eve_freew_draw_t _draw;
    uint8_t tag0;
    uint8_t tagN;
} EVEFreeWidget;

typedef struct EVEFreeSpec {
    eve_freew_touch_t touch;
    eve_freew_draw_t draw;
    eve_kbd_input_handler_t putc;
} EVEFreeSpec;

int eve_freew_create(EVEFreeWidget *widget, EVERect *g, EVEFreeSpec *spec);
void eve_freew_init(EVEFreeWidget *widget, EVERect *g, eve_freew_touch_t touch, eve_freew_draw_t draw, eve_kbd_input_handler_t putc);
void eve_freew_update(EVEFreeWidget *widget, eve_freew_touch_t touch, eve_freew_draw_t draw, eve_kbd_input_handler_t putc);

int eve_freew_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx);
uint8_t eve_freew_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0);