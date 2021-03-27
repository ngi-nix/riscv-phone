#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "eve_font.h"

#include "screen/window.h"
#include "screen/page.h"

#include "label.h"
#include "widget.h"
#include "freew.h"

int eve_freew_create(EVEFreeWidget *widget, EVERect *g, EVEPage *page, EVEFreeSpec *spec) {
    eve_freew_init(widget, g, page, spec->draw, spec->touch, spec->putc);

    return EVE_OK;
}

void eve_freew_init(EVEFreeWidget *widget, EVERect *g, EVEPage *page, eve_freew_draw_t draw, eve_freew_touch_t touch, eve_kbd_input_handler_t putc) {
    EVEWidget *_widget = &widget->w;

    memset(widget, 0, sizeof(EVEFreeWidget));
    eve_widget_init(_widget, EVE_WIDGET_TYPE_FREE, g, page, eve_freew_draw, eve_freew_touch, putc);
    widget->_draw = draw;
    widget->_touch = touch;
    widget->w.putc = putc;
}

void eve_freew_tag(EVEFreeWidget *widget) {
    EVEWidget *_widget = &widget->w;

    if (_widget->tagN != EVE_NOTAG) {
        eve_cmd_dl(TAG(_widget->tagN));
        _widget->tagN++;
    }
}

uint8_t eve_freew_draw(EVEWidget *_widget, uint8_t tag0) {
    EVEFreeWidget *widget = (EVEFreeWidget *)_widget;

    _widget->tag0 = tag0;
    _widget->tagN = tag0;
    widget->_draw(widget);

    return _widget->tagN;
}

int eve_freew_touch(EVEWidget *_widget, EVETouch *touch, uint16_t evt) {
    EVEFreeWidget *widget = (EVEFreeWidget *)_widget;

    return widget->_touch(widget, touch, evt);
}
