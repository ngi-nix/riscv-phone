#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "unicode.h"

#include "screen/screen.h"
#include "screen/window.h"
#include "screen/page.h"

#include "font.h"
#include "label.h"
#include "widget.h"
#include "freew.h"

int eve_freew_create(EVEFreeWidget *widget, EVERect *g, EVEFreeSpec *spec) {
    eve_freew_init(widget, g, spec->touch, spec->draw, spec->putc);

    return EVE_OK;
}

void eve_freew_init(EVEFreeWidget *widget, EVERect *g, eve_freew_touch_t touch, eve_freew_draw_t draw, eve_kbd_input_handler_t putc) {
    EVEWidget *_widget = &widget->w;

    memset(widget, 0, sizeof(EVEFreeWidget));
    eve_widget_init(_widget, EVE_WIDGET_TYPE_FREE, g, eve_freew_touch, eve_freew_draw, putc);
    eve_freew_update(widget, touch, draw, NULL);
}

void eve_freew_update(EVEFreeWidget *widget, eve_freew_touch_t touch, eve_freew_draw_t draw, eve_kbd_input_handler_t putc) {
    if (touch) widget->_touch = touch;
    if (draw) widget->_draw = draw;
    if (putc) widget->w.putc = putc;
}

int eve_freew_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx) {
    EVEFreeWidget *widget = (EVEFreeWidget *)_widget;
    EVETouch *t;
    uint16_t evt;
    int ret = 0;

    if (touch_idx > 0) return 0;

    t = eve_touch_evt(tag0, touch_idx, widget->tag0, widget->tagN - widget->tag0, &evt);
    if (t && evt) {
        ret = widget->_touch(widget, page, t, evt, tag0, touch_idx);
        if (!ret && page && page->handle_evt) ret = page->handle_evt(page, _widget, t, evt, tag0, touch_idx);
    }

    return ret;
}

uint8_t eve_freew_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0) {
    EVEFreeWidget *widget = (EVEFreeWidget *)_widget;

    widget->tag0 = tag0;
    widget->tagN = widget->_draw(widget, page, tag0);

    return widget->tagN;
}
