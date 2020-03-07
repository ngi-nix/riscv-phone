#include <stdlib.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen/screen.h"
#include "screen/tile.h"
#include "screen/page.h"
#include "screen/font.h"

#include "widget.h"
#include "page.h"
#include "text.h"

static const size_t _eve_wsize[] = {
    0,
    sizeof(EVETextWidget),
    sizeof(EVEPageWidget)
};

void eve_widget_init(EVEWidget *widget, uint8_t type, int16_t x, int16_t y, uint16_t w, uint16_t h, eve_widget_touch_t touch, eve_widget_draw_t draw, eve_kbd_input_handler_t putc) {
    widget->type = type;
    widget->x = x;
    widget->y = y;
    widget->w = w;
    widget->h = h;
    widget->touch = touch;
    widget->draw = draw;
    widget->putc = putc;
}

EVEWidget *eve_widget_next(EVEWidget *widget) {
    char *_w = (char *)widget;
    return (EVEWidget *)(_w + _eve_wsize[widget->type]);
}