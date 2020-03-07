#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen/screen.h"
#include "screen/tile.h"
#include "screen/page.h"

#include "widget.h"
#include "page.h"

void eve_pagew_init(EVEPageWidget *widget, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t font_id, char *title, EVEPage *page) {
    memset(widget, 0, sizeof(EVEPageWidget));
    eve_widget_init(&widget->w, EVE_WIDGET_TYPE_PAGE, x, y, w, h, eve_pagew_touch, eve_pagew_draw, NULL);
    widget->font_id = font_id;
    widget->title = title;
    widget->page = page;
}

int eve_pagew_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx, EVEPageFocus *focus) {
    EVEPageWidget *widget = (EVEPageWidget *)_widget;
    EVETouch *t;
    uint16_t evt;
    int ret = 0;

    if (touch_idx > 0) return 0;

    t = eve_touch_evt(tag0, touch_idx, widget->tag, widget->tag, &evt);
    if (t && evt) {
        if (evt & EVE_TOUCH_ETYPE_TRACK_MASK) {
            if (page && page->handle_evt) page->handle_evt(page, _widget, t, evt, tag0, touch_idx);
        } else if (evt & EVE_TOUCH_ETYPE_TAG_UP) {
            widget->page->open(widget->page, page);
            if (focus) eve_page_widget_focus(focus, _widget);
        }
        ret = 1;
    }
    return ret;
}

uint8_t eve_pagew_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0) {
    EVEPageWidget *widget = (EVEPageWidget *)_widget;
    char draw = page ? eve_page_widget_visible(page, _widget) : 1;

    widget->tag = 0;
    if (draw) {
        widget->tag = tag0;
        if (widget->tag) eve_cmd_dl(TAG(widget->tag));
        eve_cmd(CMD_TEXT, "hhhhs", _widget->x, _widget->y, widget->font_id, 0, widget->title);
    }

    return widget->tag;
}
